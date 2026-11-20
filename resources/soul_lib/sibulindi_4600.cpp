// 斯布林蒂 4600 —— 魂印 2197「未来链接」（完整版）
//
// 官方魂印（DB effect_icon id=1806 / effect_id=2197，pet_id=4600）：
//   ① 战斗开始时，斯布林蒂会使己方所有其他精灵进行未来链接，未来链接初始为3级，
//      未来链接同时也视为未来奇点、未来指令参与层数、道数的计算（不占用原有上限
//      与触发原有效果）
//   ② 斯布林蒂存活于出战背包时，己方处于未来链接的精灵计算受到的攻击伤害时**不计算
//      克制倍数**，并乘以己方处于未来链接的精灵中受克制最低者的被克制倍数，触发后
//      对应此被克制倍数的精灵减少1级未来链接
//   ③ 斯布林蒂主动下场后，根据下只出战精灵的未来链接等级，依次触发：
//      3级：将自身的体力上限值同步给下只精灵 / 2级：将自身的体力值同步给下只精灵 /
//      1级：将自身的PP值同步给下只精灵
//   ④ 己方其他精灵死亡时失去全部未来链接，斯布林蒂被击败时消除己方所有未来链接，
//      每消除1级对应的精灵恢复300点体力与所有技能1点PP值（boss无效）
//
// 实现口径（2026-09-24 批 9）：
//   - 层数账本：每只被链接精灵 `soulmark_storage[kFutureLinkLevelKey]`（共享头
//     include/effects/future_link.h，下场保留；键位给 2198/2041 互认预留）。
//     斯布林蒂本人**不在链接池**（官方"使己方所有其他精灵"）。
//   - ②的伤害接管 = SKILL_EFFECT 时点（伤害公式之前）把 `ws.restraint_view[攻击方]`
//     直写为池内最低被克制倍数——**不读原倍数、无条件覆盖**。这就是"不计算克制倍数"
//     的引擎表达，也是"拦住克制锁"的机制：光之惩戒·英卡洛斯（1164）"克制倍数至少为2"
//     的节点同桶先写（source 1164 < 2197，timed_bucket.h 按 key 升序执行），链接镜像
//     节点后写覆盖 → 锁没有作用对象。**镜像节点注册进对手（攻击方）桶**：命中族时点
//     只跑 mover 桶（lingchao_4599 镜像同款），TEAM scope + 载荷守卫保生命周期。
//     池内倍数按"攻击方技能系别视图 × 各链接精灵元素"现算（在场那只用 ws 元素视图，
//     场下用面板元素）；并列最低取**槽位最小**者扣级（官方未定义并列口径，v1 拍板）。
//   - ②的触发门：受击者（对手在场宠）本人层级 ≥1 才接管——官方"己方**处于未来链接**
//     的精灵计算受到的攻击伤害时"；斯布林蒂存活于出战背包（find_pet_with_soulmark，
//     hp>0 天然剔消逝）。
//   - 每次攻击结算消耗 1 级（变威力重算复用同一 restraint_view，不双扣；miss/被盔
//     出口 allowAttackDamagePipeline=false 不写不扣——没有伤害计算就没有"触发"）。
//   - ③的"主动下场"判定：perform_switch 统一发 EVENT_ENTER_STAGE（主动中切与死亡
//     换宠都走它），用 plugin_storage 在场快照定位**离场者**；离场者是本人且
//     pet_death_notified=false（非死亡换宠）才算"主动下场"。同步按登场者层级累计：
//     ≥3 上限（走 raise/reduce_max_hp_flat 原语）→ ≥2 当前体力 → ≥1 全技能 PP。
//   - ④死亡账：EVENT_DEATH watcher——队友死清自己全部层级；本人被击败清全队层级，
//     每清 1 级该精灵回 300 体力（在场走 heal_amount 原语，场下直写、不设复活）+
//     所有技能 +1 PP（-1 无限制跳过，clamp maxPP）。回补是**增益**，不吃
//     off_field_stats_protect（那道门只护"减少"）。
//   - ①的"视为未来奇点/未来指令"互认**待做**（2198/2041 尚未实现；键位已预留）。
// 待做：②的"受克制最低者"并列口径官方观察；强制离场（驱逐类）是否算"主动下场"。
#include <effects/future_link.h>

#include "common.h"

namespace {

constexpr int kMarkId = 2197;                 // effect_icon.effect_id = 魂印注册键
constexpr int kMirrorNodeId = kMarkId * 100 + 90;   // 镜像节点 id（key 高位=source 2197 > 1164 锁）
constexpr int kArmGuardKey = 990219701;       // plugin_storage[+side]：watcher/镜像接线守卫
constexpr int kOnStageSnapKey = 990219711;    // plugin_storage[+side]：在场快照（离场者判定）

EffectResult sibulindi_2197_link_restraint(BattleContext* ctx, const EffectArgs& args);

// ── ③ 主动下场同步（离场者=本人、非死亡换宠）──────────────────────
void sync_stats_to_incoming(BattleContext* ctx, int side, int sib_slot, int incoming_slot) {
    const int lv = future_link_level(ctx, side, incoming_slot);
    if (lv <= 0) {
        return;   // 登场者不在链接中：什么都不同步
    }
    ElfPet& sib = ctx->seerRobot[side].elfPets[sib_slot];
    ElfPet& nxt = ctx->seerRobot[side].elfPets[incoming_slot];
    if (lv >= 3) {   // 3级：体力上限值同步
        const int sib_max = resolve_pet_max_hp(sib);
        const int nxt_max = resolve_pet_max_hp(nxt);
        if (sib_max > nxt_max && g_core && g_core->raise_max_hp_flat) {
            g_core->raise_max_hp_flat(ctx, side, incoming_slot, sib_max - nxt_max);
        } else if (nxt_max > sib_max && g_core && g_core->reduce_max_hp_flat) {
            g_core->reduce_max_hp_flat(ctx, side, incoming_slot, nxt_max - sib_max, /*floor=*/1);
        }
    }
    if (lv >= 2) {   // 2级：体力值同步（在 3级 的上限同步之后取新上限封顶）
        nxt.hp = std::min(sib.hp, resolve_pet_max_hp(nxt));
    }
    if (lv >= 1) {   // 1级：PP值同步（-1 无限制不搬，clamp maxPP）
        for (std::size_t i = 0; i < nxt.skills.size() && i < sib.skills.size(); ++i) {
            const int src = sib.skills[i].pp;
            if (src >= 0) {
                nxt.skills[i].pp = std::min(src, nxt.skills[i].maxPP);
            }
        }
    }
}

// ── ② 伤害接管（镜像节点：注册在**攻击方**桶 FIRST/SECOND_SKILL_EFFECT）──
EffectResult sibulindi_2197_link_restraint(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    // arm 镜像绑参 {owner=链接方, foe=桶归属方, slot=本人槽}。
    int sib_side = -1;
    int bucket_owner = -1;
    int sib_slot = -1;
    if (args.int_args && args.int_count >= 3) {
        sib_side = args.int_args[0];
        bucket_owner = args.int_args[1];
        sib_slot = args.int_args[2];
    }
    if (sib_side < 0 || sib_side > 1 || bucket_owner != (1 - sib_side)) {
        return EffectResult::kOk;
    }
    // 当前出手方 = 本桶归属方（FIRST/SECOND 各自只有 mover 桶被执行，防御性再判）。
    int mover = -1;
    if (ctx->currentState == State::BATTLE_FIRST_SKILL_EFFECT) {
        mover = ctx->is_second_mover(0) ? 1 : 0;
    } else if (ctx->currentState == State::BATTLE_SECOND_SKILL_EFFECT) {
        if (ctx->is_second_mover(0) == ctx->is_second_mover(1)) {
            return EffectResult::kOk;
        }
        mover = ctx->is_second_mover(0) ? 0 : 1;
    } else {
        return EffectResult::kOk;
    }
    if (mover != bucket_owner || !g_core || !g_core->restraint_multiplier) {
        return EffectResult::kOk;
    }
    // 斯布林蒂存活于出战背包（hp>0，消逝/阵亡/不在背包都算失效——官方门）。
    if (ctx->find_pet_with_soulmark(sib_side, kMarkId) != sib_slot) {
        return EffectResult::kOk;
    }
    // 只认会进伤害公式的攻击技能（静态类型 + 未 miss/被盔；lingchao gate 同款判据）。
    const int idx = ctx->roundChoice[mover][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(mover).skills.size())) {
        return EffectResult::kOk;
    }
    const Skills& used = ctx->getPet(mover).skills[static_cast<std::size_t>(idx)];
    if (used.type == SkillType::Attribute) {
        return EffectResult::kOk;
    }
    if (!ctx->ws.skill_resolution_flags[mover].allowAttackDamagePipeline) {
        return EffectResult::kOk;
    }
    // 受击者本人必须处于链接中（层级 ≥1），否则按正常克制结算。
    const int defender_slot = ctx->on_stage[sib_side];
    if (future_link_level(ctx, sib_side, defender_slot) <= 0) {
        return EffectResult::kOk;
    }
    // 池内最低被克制倍数：所有"处于链接"（层级≥1、存活、未消逝）的己方精灵，
    // 按攻击方技能系别视图 × 各自元素现算，取 min；并列取槽位最小（v1 拍板）。
    double best = -1.0;
    int best_slot = -1;
    for (int slot = 0; slot < 6; ++slot) {
        if (future_link_level(ctx, sib_side, slot) <= 0) {
            continue;
        }
        const ElfPet& ally = ctx->seerRobot[sib_side].elfPets[slot];
        if (ally.hp <= 0 || ctx->is_vanished(sib_side, slot)) {
            continue;
        }
        // 在场那只用 ws 元素视图（吃对局内变系别），场下用面板元素。
        int def_elem[2];
        if (slot == defender_slot) {
            def_elem[0] = ctx->ws.view_elementalAttributes[sib_side][0];
            def_elem[1] = ctx->ws.view_elementalAttributes[sib_side][1];
        } else {
            def_elem[0] = ally.elementalAttributes[0];
            def_elem[1] = ally.elementalAttributes[1];
        }
        const double m = g_core->restraint_multiplier(ctx->ws.skill_element_view[mover], def_elem);
        if (best < 0.0 || m < best) {
            best = m;
            best_slot = slot;
        }
    }
    if (best_slot < 0) {
        return EffectResult::kOk;
    }
    // 接管：无条件覆盖（不读原倍数）——"不计算克制倍数"；同桶更早的"克制倍数至少
    // 为2"锁（source 1164）已被本次覆盖拦下。
    ctx->ws.restraint_view[mover] = best;
    // 触发扣级：被选用倍数对应的那只精灵 -1 级（变威力重算复用同一视图，不双扣）。
    future_link_set_level(ctx, sib_side, best_slot,
                          future_link_level(ctx, sib_side, best_slot) - 1);
    return EffectResult::kOk;
}

// ── ④ 死亡账 + ③ 登场账（watcher 载荷）───────────────────────────
void on_death(BattleContext* wctx, const BattleEvent& ev, int sib_side, int sib_slot) {
    if (ev.target != sib_side || ev.slot < 0 || ev.slot > 5) {
        return;
    }
    if (ev.slot == sib_slot) {
        // 斯布林蒂被击败：全队清层 + 每清 1 级回 300 体力、全技能 +1 PP。
        for (int slot = 0; slot < 6; ++slot) {
            if (slot == sib_slot) {
                continue;
            }
            const int lv = future_link_level(wctx, sib_side, slot);
            if (lv <= 0) {
                continue;
            }
            future_link_set_level(wctx, sib_side, slot, 0);
            ElfPet& ally = wctx->seerRobot[sib_side].elfPets[slot];
            if (ally.hp <= 0) {
                continue;   // 阵亡的不回补（回血不复活）
            }
            if (slot == wctx->on_stage[sib_side] && g_core && g_core->heal_amount) {
                g_core->heal_amount(wctx, sib_side, 300 * lv);
            } else {
                ally.hp = std::min(ally.hp + 300 * lv, resolve_pet_max_hp(ally));
            }
            for (Skills& s : ally.skills) {
                if (s.pp >= 0) {
                    s.pp = std::min(s.maxPP, s.pp + lv);
                }
            }
        }
        return;
    }
    // 己方其他精灵死亡：失去全部未来链接。
    future_link_set_level(wctx, sib_side, ev.slot, 0);
}

void on_enter_stage(BattleContext* wctx, const BattleEvent& ev, int sib_side, int sib_slot) {
    const int actor = ev.actor;
    if (actor < 0 || actor > 1) {
        return;
    }
    const int snap_key = kOnStageSnapKey + actor;
    int departer = -1;
    if (wctx->plugin_storage.count(snap_key)) {
        departer = any_int_or(wctx->plugin_storage[snap_key]);
    }
    const int entering = wctx->on_stage[actor];
    if (departer >= 0 && departer <= 5 && departer != entering) {
        // 离场者是本人、且不是死亡换宠 → "主动下场"，按登场者层级做 ③ 同步。
        if (departer == sib_slot && !wctx->pet_death_notified[actor][departer]) {
            sync_stats_to_incoming(wctx, actor, sib_slot, entering);
        }
    }
    wctx->plugin_storage[snap_key] = entering;
}

// ── ① 开局接线（ENTER_EXIT_STAGE，ROSTER：存活于背包即注册）─────────
EffectResult sibulindi_2197_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0) {
        return EffectResult::kOk;
    }
    const int guard = kArmGuardKey + owner;
    if (ctx->plugin_storage.count(guard)) {
        return EffectResult::kOk;   // watcher/镜像整场只接一次（arm 每次登场都会跑）
    }
    ctx->plugin_storage[guard] = true;

    // 初始 3 级：其他 5 槽各给一次（键不存在才给——死亡清账后复活不复得）。
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == self_slot) {
            continue;
        }
        auto& storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
        if (!storage.count(kFutureLinkLevelKey)) {
            storage[kFutureLinkLevelKey] = kFutureLinkMaxLevel;
        }
    }
    // 在场快照：主动下场的离场者判定基准（演出：开局那只）。
    ctx->plugin_storage[kOnStageSnapKey + owner] = ctx->on_stage[owner];

    // ② 镜像注册：SKILL_EFFECT 时点只跑 mover 桶 → 接管节点必须挂进对手（攻击方）
    //    桶。TEAM + 载荷守卫保生命周期（lingchao 镜像同款）；node id 高位 source=2197
    //    决定它排在 1164 锁节点**之后**执行（桶按 key 升序）→ 后写覆盖 = 拦截。
    {
        const int foe = 1 - owner;
        auto make_mirror = [&](State st) {
            Effect wrapper(kMirrorNodeId, 0, foe, /*left_round=*/-1,
                           EffectArgs(std::vector<int>{owner, foe, self_slot}),
                           &sibulindi_2197_link_restraint);
            auto ce = std::make_unique<ContinuousEffect>(
                wrapper, st, foe, /*duration=*/-1, ctx->roundCount);
            ce->source_id_ = kMarkId;
            ce->scope_ = EffectScope::TEAM;   // 锚在对手 epoch 会错清；守卫在载荷里
            ce->once_ = false;
            ctx->register_soulmark_effect(st, foe, std::move(ce));
        };
        make_mirror(State::BATTLE_FIRST_SKILL_EFFECT);
        make_mirror(State::BATTLE_SECOND_SKILL_EFFECT);
    }

    // ④ 队友死清层 / 本人被击败全清+回补。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner, self_slot](BattleContext* wctx, const BattleEvent& ev) {
            on_death(wctx, ev, owner, self_slot);
        },
        WatcherScope::TEAM);

    // ③ 登场账：维护在场快照 + 主动下场同步（主动中切与死亡换宠共同漏斗）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner, self_slot](BattleContext* wctx, const BattleEvent& ev) {
            on_enter_stage(wctx, ev, owner, self_slot);
        },
        WatcherScope::TEAM);

    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &sibulindi_2197_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER)});
