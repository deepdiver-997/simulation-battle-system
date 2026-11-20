// 圣光格劳瑞（神谕 4004）魂印 2532 —— 神圣之霆 / 荣光之裁（精灵王线，2026-09-24）
//
// 官方全文（effect_icon 2121 / effect 2532）：
//   自身登场时，获得1道神圣之霆，然后自身每有1层神圣之霆为对手附加1层荣光之裁、
//   对手下1次使用攻击技能时将对手所处的异常状态转化为回合数三倍的麻痹、
//   对手下1次使用属性技能时令对手麻痹一回合、对手下1个未使用技能的当回合结束时
//   令对手下回合攻击技能先制无法高于0
//   | 战斗阶段结束时，若自身体力高于最大体力的1/2则下次攻击造成的伤害提升50%，
//   若自身体力低于最大体力的1/2则令对手麻痹，然后若当回合对手免疫过或自身受到过麻痹状态，
//   则当回合结束后解除自身所处的异常状态并消除对手的回合类效果与能力提升状态
//   | （boss无效）
//
// 荣光之裁（语料 idx=19）：绑定对手，持有者有免疫攻击效果则消耗 1 层令其失效、
// **视为不处于异常**（K6 遮蔽，只影响插件条件查询面）。
// ⚠️ 简化/待实测（交接文档同步）：
//   · "对手下1次攻击/属性技→麻痹转化"两条弹控未做（需次数型封属票挂条件分支，批 B 遗留）；
//   · 霆满 6 下场保留口径按"保留"实现；麻痹触发计数 +1 预算未建模；
//   · 裁的"免疫攻击效果失效"以耗 1 层 + PENETRATE_ATTACK 窗口近似（真实交互是摘对方的
//     IMMUNE 票，RuleCenter 无按持有者摘票的接口）。
#include "common.h"
#include "../gelaorui_runtime.h"

namespace {

constexpr int kMark = 2532;

inline bool onstage_2532(BattleContext* ctx, int owner) {
    return ctx->on_stage[owner] >= 0 && ctx->on_stage[owner] < 6
        && ctx->getPet(owner).soulMark.id == kMark;
}

// 获得霆：每 +1 层 → 对手 +1 裁（镜像发放）；同步 K6 遮蔽开关。
void gain_ting(BattleContext* ctx, int owner, int n) {
    int ting = gla_pub_get(ctx->plugin_storage, kGlaPubTing + owner) + n;
    if (ting > 6) {
        ting = 6;
    }
    gla_pub_set(ctx->plugin_storage, kGlaPubTing + owner, ting);
    const int opp = 1 - owner;
    int cai = gla_cai_get(g_core, ctx, opp) + n;
    if (cai > 6) {
        cai = 6;
    }
    gla_cai_set(g_core, ctx, opp, cai);
}

EffectResult gla_on_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // early 节点防双跑守卫：开局 early（activate）+ 桶执行各跑一次，on_stage_storage
    // 下场即清 → 每次登场恰好执行一次（+1 霆/弹控武装不幂等，必须挡）。
    auto& presence = ctx->getPet(owner).on_stage_storage;
    constexpr int kPresenceKey = kMark * 100 + 94;
    if (presence.count(kPresenceKey)) {
        return EffectResult::kOk;
    }
    presence[kPresenceKey] = true;
    gain_ting(ctx, owner, 1);   // ① 登场 +1 霆（镜像 +1 裁）

    // ② 弹控三条武装（2026-09-25 遗留批）："对手下1次…"——每次登场（重新）武装。
    //    消费判定在 ROUND_END（gla_traps_round_end），按当回合对手行为结清。
    gla_pub_set(ctx->plugin_storage, kGlaPubTrapAttack + 1 - owner, 1);
    gla_pub_set(ctx->plugin_storage, kGlaPubTrapAttr + 1 - owner, 1);
    gla_pub_set(ctx->plugin_storage, kGlaPubTrapIdle + 1 - owner, 1);

    // 荣光之裁×盔精确化（2026-09-25）：**遮蔽标记票**（ARMOR_SUPPRESS）——开局授一次；
    // 消费通道 = notify 遮蔽命中时发 blocked=false 事件（grant_id=本标记），下方 watcher
    // 按 grant_id 精确匹配后自扣裁层（idx=19"有免疫攻击效果才消耗"由 condition 现场保证）。
    // 旧 MOVE_RIGHT 穿透凭证线删除：遮蔽是持有方 debuff，任何攻击者都受益，不再限格劳瑞。
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kMarkerKey = kMark * 100 + 93;
    const int opp = 1 - owner;
    if (!storage.count(kMarkerKey)) {
        const int marker_id = ctx->grant_armor_suppress(
            owner, opp, kMark,
            [opp](BattleContext* c, int, int, bool) {
                return gla_cai_get(g_core, c, opp) > 0;
            });
        storage[kMarkerKey] = marker_id;
    }
    const int marker_id =
        storage.count(kMarkerKey) ? any_int_or(storage[kMarkerKey]) : 0;

    constexpr int kWatcherKey = kMark * 100 + 92;
    if (!storage.count(kWatcherKey)) {
        storage[kWatcherKey] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_ARMOR_RESOLVED, owner, ctx->roundCount, 0, /*once=*/false,
            [owner, marker_id](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.blocked || ev.grant_id != marker_id) {
                    return;   // 只认自己这张遮蔽标记的结算（2270 grant_id 匹配纪律）
                }
                const int opp = 1 - owner;
                int cai = gla_cai_get(g_core, wctx, opp);
                if (cai <= 0) {
                    return;
                }
                gla_cai_set(g_core, wctx, opp, cai - 1);
                // 层尽 → K6 遮蔽自然解除（查询时拉取，无需主动撤销）
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

EffectResult gla_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_2532(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    // ① "对手下1个未使用技能"触发补偿：对手本回合攻先 ≤0（2492 庇的同款
    //    priority_nullified 清零近似；"无法高于0"按清零记偏差，见交接文档）。
    if (gla_pub_get(ctx->plugin_storage, kGlaPubIdleUntil + opp) >= ctx->roundCount) {
        ctx->ws.priority_nullified[opp] = true;
    }
    // ② 2551 后半句：击败武装 → 对手属性技命中失效（次数大额近似"直到霆减少"；
    //    霆减少在下面检测到时清旗停止重授，存量次数型条目随对局自然衰减——记偏差）。
    if (gla_pub_get(ctx->plugin_storage, kGlaPubBlind + opp) > 0) {
        if (gla_pub_get(ctx->plugin_storage, kGlaPubTing + owner)
            < gla_pub_get(ctx->plugin_storage, kGlaPubBlindTing + owner)) {
            gla_pub_set(ctx->plugin_storage, kGlaPubBlind + opp, 0);   // 霆已减少 → 解除
        } else if (g_core->hit_effect_invalid) {
            g_core->hit_effect_invalid(ctx, opp, HitInvalidMode::kEffectsOnly,
                                       /*count=*/9, /*is_attribute_skill=*/true, 2551);
        }
    }
    return EffectResult::kOk;
}

// 弹控三条消费（ROUND_END，抢在高低体分支之前）：按对手当回合行为结清武装；
// 麻痹触发成功 → 霆预算 +1（gain_ting 镜像 +1 裁）。
namespace {

// 挂在匿名空间：把对手身上所有现代异常转为麻痹——"回合数三倍"按**最大剩余回合**取
// （麻痹单实例、同槽替换，多笔转化落成一笔取最长；CoreApi 无单条消除，先全消除再施加）。
// ⚠️ 读数走**原始 end_round**（BattleContext 查询），不走 CoreApi::has_active_anomaly：
//    对手持有荣光之裁时 K6 遮蔽会把它自己的异常条件查询面压成 false，弹控是格劳瑞
//    自己施加的转化，不吃自己遮蔽的假阴性（2026-09-25 场景 F 实测修正）。
int trap_transform_anomalies_to_paralysis(BattleContext* ctx, int opp, int owner) {
    if (!g_core || !g_core->get_abnormal_status_end_round || !g_core->apply_anomaly) {
        return 0;
    }
    int max_remaining = 0;
    for (int id = 0; id <= 43; ++id) {
        if (id == static_cast<int>(AbnormalStatusId::Paralysis)) {
            continue;   // 已是麻痹的不参与"转化"（自身无剩余回合可乘）
        }
        const int remaining =
            g_core->get_abnormal_status_end_round(ctx, opp, id) - ctx->roundCount;
        if (remaining > max_remaining) {
            max_remaining = remaining;
        }
    }
    if (max_remaining < 1) {
        return 0;   // 对手不处于（可转化的）异常 → 转化未触发、无麻痹无预算
    }
    if (g_core->dispel_active_anomalies) {
        g_core->dispel_active_anomalies(ctx, opp);
    }
    // +1：ROUND_END 施加会被同回合扣减点吃 1 层（"回合数三倍"字面 + 时点修正，待实测）
    if (g_core->apply_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Paralysis),
                              max_remaining * 3 + 1, owner)
        == ApplyAnomalyResult::SUCCESS) {
        return 1;
    }
    return 0;
}

// 弹控消费（ROUND_END：两方都已行动，逐侧结清武装）。
// ⚠️ 施加量 +1：ROUND_END 施加的异常会被同回合扣减点吃 1 层，+1 才能净覆盖
//    "对手下回合"（实测 F 段修正； transform 的"回合数三倍"同理 3×n+1）。
EffectResult gla_traps_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;   // 格劳瑞的对手（弹控目标；行为与武装键都在对手侧）
    const bool selected = ctx->roundChoice[opp][0] == 1 /*SELECT_SKILL*/;
    const bool used = selected && ctx->ws.skill_pp_cost_consumed[opp];
    if (!used) {
        // "对手下1个未使用技能"：换宠/嗑药/被控都算未用 → 下回合对手攻先 ≤0。
        if (gla_pub_get(ctx->plugin_storage, kGlaPubTrapIdle + opp) > 0) {
            gla_pub_set(ctx->plugin_storage, kGlaPubIdleUntil + opp, ctx->roundCount + 1);
            gla_pub_set(ctx->plugin_storage, kGlaPubTrapIdle + opp, 0);
        }
        return EffectResult::kOk;
    }
    const int slot = ctx->roundChoice[opp][1];
    bool is_attack = false;
    if (slot >= 0 && slot < 5) {
        is_attack = ctx->getPet(opp).skills[slot].type != SkillType::Attribute;
    }
    int paralysis_wins = 0;
    if (is_attack && gla_pub_get(ctx->plugin_storage, kGlaPubTrapAttack + opp) > 0) {
        // 下1次攻击技：把对手所处异常状态转化为回合数三倍的麻痹。
        paralysis_wins += trap_transform_anomalies_to_paralysis(ctx, opp, owner);
        gla_pub_set(ctx->plugin_storage, kGlaPubTrapAttack + opp, 0);
    } else if (!is_attack && gla_pub_get(ctx->plugin_storage, kGlaPubTrapAttr + opp) > 0) {
        // 下1次属性技：麻痹 1 回合（+1 抵扣当回合扣减点）。
        if (g_core->apply_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Paralysis),
                                  2, owner)
            == ApplyAnomalyResult::SUCCESS) {
            ++paralysis_wins;
        }
        gla_pub_set(ctx->plugin_storage, kGlaPubTrapAttr + opp, 0);
    }
    // 霆预算：麻痹触发成功 → 霆 +1（gain_ting 镜像 +1 裁；cap 6 收口）。
    for (int i = 0; i < paralysis_wins; ++i) {
        gain_ting(ctx, owner, 1);
    }
    return EffectResult::kOk;
}

}  // namespace

// 阶段结束：高体→下次攻击伤害+50%（本回合内加算槽近似）；低体→令对手麻痹；
// 若当回合对手免疫过麻痹或自身受到过麻痹 → 解自身异常 + 消对手回合类效果与能力提升。
// 弹控三条的消费判定先于高低体分支跑（同属"回合结束时"语料，先结清武装再看状态分支）。
EffectResult gla_phase_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_2532(ctx, owner)) {
        return EffectResult::kOk;
    }
    // 弹控三条先于高低体分支结清（同属"回合结束时"语料，武装消费看当回合行为）。
    gla_traps_round_end(ctx, args);
    const int max_hp = resolve_pet_max_hp(ctx->getPet(owner));
    const bool high = ctx->getPet(owner).hp > max_hp / 2;
    if (high) {
        ctx->ws.damage_add_pct[owner] += 50;   // "下次攻击"以本回合加算槽近似（待实测）
    } else if (g_core && g_core->apply_anomaly) {
        // 低体麻痹：触发成功同样进霆预算（"麻痹触发成功计数"族口径）。
        if (g_core->apply_anomaly(ctx, 1 - owner,
                                  static_cast<int>(AbnormalStatusId::Paralysis), 1, owner)
            == ApplyAnomalyResult::SUCCESS) {
            gain_ting(ctx, owner, 1);
        }
    }
    if (ctx->ws.gla_parity_flag[owner] && g_core) {
        if (g_core->dispel_active_anomalies) {
            g_core->dispel_active_anomalies(ctx, owner);
        }
        if (g_core->break_round_effects) {
            g_core->break_round_effects(ctx, 1 - owner);
        }
        if (g_core->clear_stat_boosts_as) {
            g_core->clear_stat_boosts_as(ctx, 1 - owner, owner);
        }
    }
    ctx->ws.gla_parity_flag[owner] = false;
    return EffectResult::kOk;
}

}  // namespace

// ⚠️ on_enter 必须 early=true（官方"登场 +1 霆/弹控三条武装"= 每次登场重发；
//    函数体内注释本就写"每次登场（重新）武装"。非 early 节点只在开局执行一次，
//    切上场后不再触发——2026-09-26 登场语义批次统一修正，见 scenario_130 D 组。
//    遮蔽标记票/watcher 段有 storage 守卫，重复执行安全）。
SOUL_PROGRAM(2532, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &gla_on_enter,
                     /*once=*/false, /*early=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &gla_round_start),
                    SoulMarkNodeRef(State::BATTLE_ROUND_END, &gla_phase_end)});
