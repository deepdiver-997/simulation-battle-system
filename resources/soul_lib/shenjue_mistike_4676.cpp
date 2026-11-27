// 神觉·米斯蒂克 4676 完整魂印（4399 图鉴 996677 官方全文 + 机制解析语料 981776506821804033
// + B站解析 993011144832581680 + 用户 2026-09-20 口径）：
//   ① 每次登场后，对手2回合内属性技能无效，攻击技能无法附加异常状态与能力下降状态；
//      若登场当回合对手未使用技能则回合结束后为对手附加2道蚩庸之锁
//   ② 对手每次使用技能时随机2项能力等级-1，未触发则自身当回合受到的攻击伤害、固定伤害、
//      百分比伤害减少50%；然后若本次技能为攻击技能则自身下回合所有技能先制+1且必定命中，
//      若本次技能为属性技能则附加对手最大体力1/3的百分比伤害且对手下次受到的攻击伤害提升100%
//   ③ 蚩庸之锁：米斯蒂克在场时每有1层对手视为命中等级额外-1；米斯蒂克不在场时每回合减少
//      1层，且直到消失对手属性技能无效、攻击技能无法附加异常状态与能力下降状态（解析补充：
//      不在场组与登场组一样含弱化）；最高叠加3道。印记绑定对方在场精灵，下场不保留。
//
// 关键口径（用户 2026-09-20 拍板）：
//   ★ 附加禁令 = **查询式保护**（RuleCategory::ATTACH_BAN 票，施加原语入口查询），不是给
//     对面全队上一段免疫——票 coverage 只覆盖双方行动段（机制解析："范围大概在对方行动
//     开始时到对方行动结束时"，回合开始前/结束后的异常与弱化、克雷二段麻痹都挡不住）；
//     古早主动毒**不拦**；弹控**弹回来**的异常不拦（depth==1 豁免，枫眠弹控反弹睡眠）。
//   ★ 封属走 **SKILL_BAN 查询票**（官方口径"不是回合类效果、不可被断回合"；SEAL 回合型
//     票会被断回合清掉，表达不了）——notify 响应，SKILL_INVALID 补偿链路照常。
//   ★ "次数增伤"（对手下次受到的攻击伤害提升100%）= 通用增伤·加法槽一次性：
//     **只有使用了攻击技能就会消耗**（官方修复口径；miss/打盔也算"使用了"），
//     按 ws.skill_type_view 判（存在技能转换，以视图为准）；
//     绑定授予时对手在场精灵，其下场即失效；米斯蒂克自己换人不受影响。
//   ★ "未使用技能"判据 = 对手本回合没有技能结算（ws.skill_resolution_ready==false）——
//     换宠/嗑药/被控跳过都算未使用（解析确认）；miss/打盔/被封属 = 已使用。
//   ★ 随机2项-1 "未触发" = 两项都没改变数值（-6 再弱化还是 -6 → 单纯判断能力变化）；
//     任一成功即算触发、无补偿。弱化在对手技能效果和伤害结算之前（事件在 ON_SKILL_HIT
//     桶后 drain、先于 SKILL_EFFECT 桶——天然满足"早于消回合补偿次免"的解析口径）。
//   ★ ②组触发为常驻（原文不限定场）；锁的命中-1才要求"米斯蒂克在场"。
//   ★ ①组登场窗口锚定登场回合（先手/中切/死切都一样），米斯蒂克换下场窗口不中断；
//     每次重新登场刷新窗口与覆盖键。
#include "common.h"
#include "../mistike_runtime.h"

namespace {

constexpr int kMarkId = kMistikeMarkId;

// ── 运行时（只在本 dylib 内用；跨库只走 mistike_runtime.h 的 int 发布槽）──
struct MistikeRuntime {
    bool watchers_armed = false;
    bool pink_registered = false;
    int entry_round = -1;       // 最近一次登场的回合（每次登场刷新）
    // 持锁槽位追踪走发布槽 mistike_pub_lock_slot（魂印与 2211 共写，跨 dylib 可见）。
    bool boost_armed = false;   // "对手下次受到的攻击伤害提升100%" 已武装
    int boost_slot = -1;        // 增伤绑定的对方精灵槽（其下场即失效）
    int haste_round = -1;       // "下回合所有技能先制+1且必定命中"的生效回合
    int reduce50_round = -1;    // "当回合受到的攻击/固定/百分比伤害减少50%"生效回合
    int off_seal_handle = 0;    // 锁期 SKILL_BAN 票柄（0 = 未授）
    int off_ban_anomaly = 0;    // 锁期 ATTACH_BAN(异常) 票柄
    int off_ban_drop = 0;       // 锁期 ATTACH_BAN(弱化) 票柄
};

constexpr int kRtKeyBase = kMarkId * 100;   // +side

MistikeRuntime& mist_rt(BattleContext* ctx, int side) {
    auto it = ctx->plugin_storage.find(kRtKeyBase + side);
    if (it == ctx->plugin_storage.end()) {
        it = ctx->plugin_storage.emplace(kRtKeyBase + side, MistikeRuntime{}).first;
    }
    return std::any_cast<MistikeRuntime&>(it->second);
}

int host_slot(BattleContext* ctx, int side) {
    return ctx->find_pet_with_soulmark(side, kMarkId);
}

// 她是否"在场"（存活且在场上）——锁的命中-1 / 锁期禁令的判定轴。
bool she_on_stage(BattleContext* ctx, int side) {
    const int slot = host_slot(ctx, side);
    return slot >= 0 && ctx->on_stage[side] == slot
        && ctx->seerRobot[side].elfPets[slot].hp > 0;
}

int lock_count(BattleContext* ctx, int opp_side, int slot) {
    if (slot < 0) {
        return 0;
    }
    return get_mark_count(ctx->seerRobot[opp_side].elfPets[slot], mistike_lock_name());
}

void set_lock_count(BattleContext* ctx, int opp_side, int slot, int n) {
    if (slot < 0) {
        return;
    }
    mistike_publish_lock_slot(ctx->plugin_storage, opp_side, n > 0 ? slot : -1);
    ElfPet& pet = ctx->seerRobot[opp_side].elfPets[slot];
    if (n <= 0) {
        remove_mark(pet, mistike_lock_name());
    } else {
        Mark* mark = find_mark(pet, mistike_lock_name());
        if (mark) {
            mark->count = n;
        } else {
            pet.marks.emplace_back(0, mistike_lock_name(), n);   // Mark(id, name, count)
        }
    }
    mistike_publish_locks(ctx->plugin_storage, opp_side, std::max(0, n));
}

// 登场组/锁期禁令的时点覆盖：双方**行动段**（ACTION_START..AFTER_ACTION_END，不含
// EXTRA_ACTION/MOVER_DEATH）——"从对方行动开始时到对方行动结束时"的官方窄窗口。
uint64_t action_window_coverage() {
    uint64_t cov = 0;
    for (int s = static_cast<int>(State::BATTLE_FIRST_ACTION_START);
         s <= static_cast<int>(State::BATTLE_FIRST_AFTER_ACTION_END); ++s) {
        cov |= (1ULL << (s + 1));
    }
    for (int s = static_cast<int>(State::BATTLE_SECOND_ACTION_START);
         s <= static_cast<int>(State::BATTLE_SECOND_AFTER_ACTION_END); ++s) {
        cov |= (1ULL << (s + 1));
    }
    return cov;
}

// ── ① 登场组禁令（source_effect_id = kMarkId；窗口 rounds=2 锚定登场回合，TEAM 域）──
// ⚠️ 与锁期禁令（source_effect_id = kMarkId*10+1，rounds=0）用不同覆盖键——两条并存时
//   （登场窗口内她切下场且锁还在）互不覆盖、各自独立到期。
void grant_entry_bans(BattleContext* ctx, int side, int opp) {
    ctx->rule_center_.grant_skill_ban(/*source=*/side, /*source_effect_id=*/kMarkId,
                                      /*target=*/opp, /*rounds=*/2, ctx->roundCount,
                                      EffectScope::TEAM);
    ctx->rule_center_.grant_attach_ban(side, kMarkId, opp, RuleCenter::kAttachBanAnomaly,
                                       action_window_coverage(), /*rounds=*/2, ctx->roundCount,
                                       EffectScope::TEAM);
    ctx->rule_center_.grant_attach_ban(side, kMarkId, opp, RuleCenter::kAttachBanStatDrop,
                                       action_window_coverage(), /*rounds=*/2, ctx->roundCount,
                                       EffectScope::TEAM);
}

// ── ③ 锁期禁令（rounds=0 不过期，靠 revoke；她回场/锁清零即撤）──────────
void sync_lock_period_bans(BattleContext* ctx, int side, int opp, MistikeRuntime& st) {
    const bool active = !she_on_stage(ctx, side) && lock_count(ctx, opp, mistike_pub_lock_slot(ctx->plugin_storage, opp)) > 0;
    if (active) {
        st.off_seal_handle = ctx->rule_center_.grant_skill_ban(
            side, /*source_effect_id=*/kMarkId * 10 + 1, opp, /*rounds=*/0, ctx->roundCount,
            EffectScope::TEAM);
        st.off_ban_anomaly = ctx->rule_center_.grant_attach_ban(
            side, kMarkId * 10 + 1, opp, RuleCenter::kAttachBanAnomaly,
            action_window_coverage(), /*rounds=*/0, ctx->roundCount, EffectScope::TEAM);
        st.off_ban_drop = ctx->rule_center_.grant_attach_ban(
            side, kMarkId * 10 + 1, opp, RuleCenter::kAttachBanStatDrop,
            action_window_coverage(), /*rounds=*/0, ctx->roundCount, EffectScope::TEAM);
    } else {
        if (st.off_seal_handle) ctx->rule_center_.revoke(st.off_seal_handle);
        if (st.off_ban_anomaly) ctx->rule_center_.revoke(st.off_ban_anomaly);
        if (st.off_ban_drop) ctx->rule_center_.revoke(st.off_ban_drop);
        st.off_seal_handle = st.off_ban_anomaly = st.off_ban_drop = 0;
    }
}

// ── ② 减粉半边（常驻管线，回调实时读生效回合）────────────────────────
void register_reduce50_pink(BattleContext* ctx, int side, MistikeRuntime& st) {
    if (st.pink_registered) {
        return;
    }
    st.pink_registered = true;
    ctx->register_pink_damage_effect(
        PinkDamagePhase::REDUCE_EXTRA, side,
        [side](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != side) {
                return;
            }
            MistikeRuntime& s = mist_rt(c, side);
            if (s.reduce50_round != c->roundCount) {
                return;   // 非补偿回合零行为
            }
            PinkDamageResolved& pink = c->resolvedPink;
            if (pink.target != side || pink.final <= 0) {
                return;
            }
            pink.final -= pink.final / 2;   // 固定/百分比伤害 -50%
        },
        EffectScope::TEAM);
}

// ── "对手每次使用技能"触发（事件在 ON_SKILL_HIT 桶后 drain——先于其 SKILL_EFFECT 桶，
//    满足"弱化在技能效果和伤害结算之前"）────────────────────────────────
void on_opp_skill_use(BattleContext* ctx, int side) {
    const int opp = 1 - side;
    MistikeRuntime& st = mist_rt(ctx, side);
    // 随机 2 项能力等级-1（互不相同；走 stat_drop 查免弱，actor=米斯蒂克方——她不是被禁方，
    // 对手的 ATTACH_BAN 不影响本施加）。-6 再弱化还是 -6（AT_FLOOR）= 数值没变 = 未触发。
    int changed = 0;
    constexpr int kStatSlots = BattleContext::kAbilityLevelSlotCount;
    const int first = choose_soul_range(*ctx, kMarkId, 0, side, opp, 0, kStatSlots - 1);
    // The former second roll indexed the ascending sequence with the first slot omitted.
    ChanceOption remaining[kStatSlots - 1];
    int count = 0;
    for (int slot = 0; slot < kStatSlots; ++slot) {
        if (slot != first) {
            remaining[count++] = {slot, 1};
        }
    }
    const int second = choose_soul_slot(*ctx, kMarkId, 1, side, opp, remaining);
    const int slots[2] = {first, second};
    for (const int stat : slots) {
        if (g_core && g_core->stat_drop
            && g_core->stat_drop(ctx, opp, stat, 1, /*actor=*/side)
                == StatDropResult::SUCCESS) {
            ++changed;
        }
    }
    if (changed == 0) {
        // 未触发 → 自身当回合受到的攻击伤害（红）、固定伤害、百分比伤害减少50%（真伤不减）。
        st.reduce50_round = ctx->roundCount;
        ctx->ws.damage_reduce_mul[side][3] = 50;   // 红伤半边；粉伤半边走常驻管线
    }
    // 然后：按本次技能（**视图**——存在技能转换，以 skill_type_view 为准）分岔。
    if (ctx->ws.skill_type_view[opp] != static_cast<int>(SkillType::Attribute)) {
        // 攻击技能 → 自身下回合所有技能先制+1且必定命中。
        st.haste_round = ctx->roundCount + 1;
    } else {
        // 属性技能 → 立即附加对手最大体力1/3的百分比伤害（先于其效果结算，本回调时点即满足）
        // + 武装"对手下次受到的攻击伤害提升100%"（绑定其当前在场精灵）。
        const int max_hp = resolve_pet_max_hp(ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]]);
        if (g_core && g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, opp, max_hp / 3, DamageKind::PERCENT_VALUE, side);
        }
        st.boost_armed = true;
        st.boost_slot = ctx->on_stage[opp];
    }
}

// "米斯蒂克使用技能"：次数增伤的**消费点**——只有使用了攻击技能就会消耗（miss/打盔也算
// 使用了；EVENT_SKILL_INVALID 也进这里），按视图判（转换后属性技能不消耗）。
void on_her_skill_use(BattleContext* ctx, int side) {
    MistikeRuntime& st = mist_rt(ctx, side);
    if (!st.boost_armed) {
        return;
    }
    if (ctx->ws.skill_type_view[side] == static_cast<int>(SkillType::Attribute)) {
        return;   // 属性技能不消耗、不生效（官方修复口径）
    }
    ctx->ws.damage_add_pct[side] += 100;   // 通用增伤·加法槽，本击生效
    st.boost_armed = false;
}

void on_skill_event(BattleContext* ctx, int side, const BattleEvent& ev) {
    if (!ctx || (ev.actor != side && ev.actor != 1 - side)) {
        return;
    }
    if (ev.actor == 1 - side) {
        on_opp_skill_use(ctx, side);
    } else {
        on_her_skill_use(ctx, side);
    }
}

void arm_watchers(BattleContext* ctx, int side, MistikeRuntime& st) {
    if (st.watchers_armed) {
        return;
    }
    st.watchers_armed = true;
    // "对手每次使用技能" 的统一通道：命中（EVENT_HIT，属性技能也算命中）或
    // 无效/未命中（EVENT_SKILL_INVALID）——两者合起来恰好覆盖"使用了一次技能"。
    for (const EventType type : {EventType::EVENT_HIT, EventType::EVENT_SKILL_INVALID}) {
        ctx->event_center_.register_watcher(
            type, side, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [side](BattleContext* c, const BattleEvent& ev) { on_skill_event(c, side, ev); },
            WatcherScope::TEAM);
    }
}

// ── 节点①：OPERATION_ENTER_EXIT_STAGE（early, STAGE）—— 登场/离场管理 ──
EffectResult mistike_enter_exit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - side;
    MistikeRuntime& st = mist_rt(ctx, side);
    arm_watchers(ctx, side, st);
    register_reduce50_pink(ctx, side, st);

    if (she_on_stage(ctx, side)) {
        // 登场（战斗开始/切换上场）：刷新登场窗口（覆盖键重授 → register_round 锚定本回合），
        // 并撤掉锁期禁令（人回来了）。
        st.entry_round = ctx->roundCount;
        grant_entry_bans(ctx, side, opp);
    }
    sync_lock_period_bans(ctx, side, opp, st);
    return EffectResult::kOk;
}

// ── 节点②：BATTLE_ROUND_START（STAGE）—— 锁维护 + 命中视图 + 窗口/增益/必中刷新 ──
EffectResult mistike_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - side;
    MistikeRuntime& st = mist_rt(ctx, side);

    // 绑定校验：持锁精灵 / 增伤绑定精灵换了人 → 印记下场不保留、增伤绑定失效。
    const int lock_slot = mistike_pub_lock_slot(ctx->plugin_storage, opp);
    if (lock_slot >= 0 && ctx->on_stage[opp] != lock_slot) {
        set_lock_count(ctx, opp, lock_slot, 0);   // 下场不保留（解析：中切可消除；发布槽随写改 -1）
    }
    if (st.boost_armed && st.boost_slot >= 0 && ctx->on_stage[opp] != st.boost_slot) {
        st.boost_armed = false;
        st.boost_slot = -1;
    }

    // 不在场 → 每回合减少1层，直到消失（消失即撤锁期禁令）。
    if (!she_on_stage(ctx, side)) {
        const int decay_slot = mistike_pub_lock_slot(ctx->plugin_storage, opp);
        const int cur = lock_count(ctx, opp, decay_slot);
        if (cur > 0) {
            set_lock_count(ctx, opp, decay_slot, cur - 1);
        }
    }

    // 锁的命中-1：她在场时，对手（在场精灵）每有1层视为命中等级额外-1；不在场不生效。
    {
        const int cur_lock_slot = mistike_pub_lock_slot(ctx->plugin_storage, opp);
        const int locks = lock_count(ctx, opp, cur_lock_slot >= 0 ? cur_lock_slot : ctx->on_stage[opp]);
        ctx->ws.hit_level_extra[opp] = (she_on_stage(ctx, side) && cur_lock_slot >= 0) ? -locks : 0;
    }

    sync_lock_period_bans(ctx, side, opp, st);

    // "下回合先制+1且必定命中"的必中半边（凭证按回合重写；先制半边在 MOVE_RIGHT 节点）。
    if (st.haste_round == ctx->roundCount) {
        ctx->ws.must_hit_grant[side] = true;
    }
    return EffectResult::kOk;
}

// ── 节点③：BATTLE_FIRST_MOVE_RIGHT（STAGE）—— haste 回合先制+1 ──
EffectResult mistike_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    MistikeRuntime& st = mist_rt(ctx, side);
    if (st.haste_round == ctx->roundCount) {
        ctx->ws.preemptive_level[side] += 1;
    }
    return EffectResult::kOk;
}

// ── 节点④：BATTLE_ROUND_END（STAGE）—— 登场当回合对手未使用技能 → +2道锁 ──
EffectResult mistike_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    const int opp = 1 - side;
    MistikeRuntime& st = mist_rt(ctx, side);
    if (st.entry_round != ctx->roundCount) {
        return EffectResult::kOk;   // 只看登场当回合
    }
    // "未使用技能"：本回合没有技能结算（换宠/嗑药/被控跳过都算；miss/打盔/被封属=已使用）。
    if (ctx->ws.skill_resolution_ready[opp]) {
        return EffectResult::kOk;
    }
    const int cur = lock_count(ctx, opp, ctx->on_stage[opp]);
    const int add = std::min(2, kMistikeLockCap - cur);
    if (add <= 0) {
        return EffectResult::kOk;   // 已到 3 道上限
    }
    set_lock_count(ctx, opp, ctx->on_stage[opp], cur + add);   // 发布槽在 set 内更新
    sync_lock_period_bans(ctx, side, opp, st);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &mistike_enter_exit,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &mistike_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &mistike_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &mistike_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
