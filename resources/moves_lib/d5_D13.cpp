// D 组批产 · D13（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   薇尔诗 4000 第五技 向未来的祈愿 38423 缺失效果：
//     2500 此技能以自然、光、冰、超能、神灵系中的最优系别计算克制倍数（0 参）
//     1922 消除对手回合类效果，消除成功则{0}回合内对手属性技能命中效果失效（切片 [2]）
//     1880 吸取对手能力提升状态，吸取成功则自身下{0}回合造成的攻击伤害提升{1}%，若对手
//          不处于能力提升状态则自身下{2}回合先制+{3}（切片 [1,100,2,1]）
//     2351 未击败对手时令对手全属性-{0}，未触发则对手{1}回合内无法主动切换精灵（[1,2]）
//     2075 未击败对手则对手下{0}回合无法解除、反转自身能力下降状态（切片 [2]）—— **跳过**
//
//   切片与 4399 图鉴文本逐位一致（"下1回合伤害提升100%、下2回合先制+1、下2回合无法解除"）。
//
//   ⚠️ 2075 跳过（硬性禁令：需要新原语）："无法解除、反转自身能力下降"需要内核在
//   clear_stat_drops / 反转效果里挂按目标的保护闸门——RuleCenter 无此类别、stat_drop
//   原语无此类入参，moves_lib 侧无法拦截 soul_lib/其他插件调用方的消强/反转。已记录待内核补原语。
//
//   其余四支实现要点：
//   · 2500：SKILL_EFFECT 时点在五候选 {自然20, 光12, 冰9, 超能10, 神灵223}（单系 {e,0}）
//     中对防御方当前系别视图取 restraint_multiplier 最优，直写 ws.restraint_view[owner]
//     （伤害公式读到 >=0 即覆盖；默认 -1、每回合 reset；FSM 2492 武装路径见我值 >=0 自动让位）。
//   · 1922：781 同款（selection_seal.cpp）：break_round_effects==SUCCESS → seal_skill
//     (attribute=true, attack=false, hit_invalid=true) —— 命中失效（非无效补偿），029 场景语义。
//   · 1880：transfer_stat_boosts(opp→self) 搬走 >0 = 吸取成功 → NextRounds 窗口
//     （ROUND_START tick 写 damage_add_pct，通用加法通道）持续 2 回合；搬走 0（对手无提升）
//     → NextRounds 窗口（FIRST_MOVE_RIGHT tick 写 preemptive_level，880 同落点）持续 1 回合。
//   · 2351：AFTER_ACTION 桥（本回合一次；"未击败"= 对手在场宠 hp>0，582/1049 同判据）→
//     6 槽 stat_drop 各 -1（原语内查免弱/钳制）；landed==0（全被免弱挡下）→
//     grant_switch_lock 2 回合（RuleCenter 锁切票，battleFsm 主动切换校验处消费）。

#include "common.h"

#include <memory>
#include <vector>

namespace {

constexpr int kNzCount = 6;   // 官方能力 6 槽（kAbilityLevelSlotCount）

// ── 2500 五系最优克制 ──────────────────────────────────────────────────────────
EffectResult effect_2500_optimal_element(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core
        || !g_core->restraint_multiplier) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = 1 - owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    static constexpr int kCandidates[5] = {20, 12, 9, 10, 223};   // 自然/光/冰/超能/神灵
    const int def_elem[2] = {ctx->ws.view_elementalAttributes[opp][0],
                             ctx->ws.view_elementalAttributes[opp][1]};
    double best = 0.0;
    bool any = false;
    for (const int e : kCandidates) {
        const int atk_elem[2] = {e, 0};   // 单系 {e,0}（1v1 分支；防御双系走 1v2 乘积）
        const double m = g_core->restraint_multiplier(atk_elem, def_elem);
        if (!any || m > best) {
            best = m;
            any = true;
        }
    }
    if (any) {
        ctx->ws.restraint_view[owner] = best;   // >=0 即被伤害公式覆盖采用
    }
    return EffectResult::kOk;
}

// ── 1922 消回合成功 → {n}回合内对手属性技能命中效果失效 ──────────────────────────
EffectResult effect_1922_break_rounds_hit_invalid(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    // 781 同款：hit_invalid=true = "命中效果失效"（照常命中、效果不注册、不走无效补偿）。
    (void)g_core->seal_skill(ctx, owner, opp, /*effect_id=*/1922, /*attribute=*/true,
                             /*attack=*/false, /*count=*/1, /*duration_rounds=*/rounds,
                             /*penetrable=*/true, /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/true, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1880 吸取提升 → 增伤窗口 / 无提升 → 先制窗口 ────────────────────────────────
EffectResult effect_1880_damage_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[1];
    if (owner < 0 || owner > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);   // 通用增伤（加法）
    return EffectResult::kOk;
}

EffectResult effect_1880_prio_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int prio = args.int_args[1];
    if (owner < 0 || owner > 1 || prio == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += prio;   // 880 同落点（MOVE_RIGHT 桶）
    return EffectResult::kOk;
}

EffectResult effect_1880_drain_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int dmg_rounds = args.int_args[2];   // {0} 吸取成功：增伤窗口回合数
    const int pct = args.int_args[3];          // {1} 攻击伤害提升 %
    const int prio_rounds = args.int_args[4];  // {2} 无提升：先制窗口回合数
    const int prio_bonus = args.int_args[5];   // {3} 先制加成
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int moved = g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner);
    if (moved > 0) {
        // 吸取成功：下 {dmg_rounds} 回合造成的攻击伤害提升 {pct}%
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1880,
                                    dmg_rounds, {owner, pct},
                                    &effect_1880_damage_window_tick);
        return EffectResult::kOk;
    }
    // 对手不处于能力提升状态：下 {prio_rounds} 回合先制 +{prio_bonus}
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1880,
                                prio_rounds, {owner, prio_bonus},
                                &effect_1880_prio_window_tick);
    return EffectResult::kOk;
}

// ── 2351 未击败 → 全属性-{n}；全未落地（免弱/已 -6 钳底）→ 锁切 2 回合 ─────────────────────────────
EffectResult effect_2351_undeathed_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];       // {0} 全属性下降幅度（正数，逐槽传负）
    const int switch_rounds = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0 || switch_rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        std::fprintf(stderr, "[2351dbg] bridge: foe dead rc=%d\n", ctx->roundCount);
        return EffectResult::kOk;   // 击败了对手 → 两支都不给
    }
    int landed = 0;
    for (int i = 0; i < kNzCount; ++i) {
        const StatDropResult r = g_core->stat_drop(ctx, opp, i, amount, /*actor=*/self);
        if (r == StatDropResult::SUCCESS) {
            ++landed;   // 落地＝已触发；AT_FLOOR（已 -6 钳底）＝未触发（2026-10-25 口径）
        }
    }
    if (landed == 0) {
        // 未触发（全未落地：免弱挡 / 已 -6 钳底）→ 对手 {switch_rounds} 回合内无法主动切换
        ctx->rule_center_.grant_switch_lock(self, 2351, opp, switch_rounds,
                                            ctx->roundCount);
    }
    return EffectResult::kOk;
}

EffectResult effect_2351_undeathed_drop_or_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // AFTER_ACTION 桥：FIRST/SECOND 各一条，只有行动方那条跑（582/1049 同款双注册）。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2351, 0, self, 1, EffectArgs(std::vector<int>{self, opp,
                                                               args.int_args[2],
                                                               args.int_args[3]}),
                 &effect_2351_undeathed_bridge);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 2351;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2500, effect_2500_optimal_element);
SKILL_EFFECT(1922, effect_1922_break_rounds_hit_invalid);
SKILL_EFFECT(1880, effect_1880_drain_boosts);
SKILL_EFFECT(2351, effect_2351_undeathed_drop_or_lock);
// 2075 跳过：需要内核"禁解除/禁反转能力下降"保护原语（见文件头 ⚠️）。
