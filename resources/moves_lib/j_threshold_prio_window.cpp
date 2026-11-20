// J61 · 阈值先制窗族 —— 1029×17 / 1223×17 / 878×17（2026-10-03，worktree ../agents/j61）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1029（args_num=3）"造成的伤害低于{0}则下{1}回合自身先制+{2}"：
//   - 阈值门 = AFTER_ACTION 读 resolvedDamage.final（1256 内核 case 归位同款——
//     需内核 effect_register_state case 1029 → BATTLE_FIRST_AFTER_ACTION 一行）；
//   - 窗 = NextRounds({1}) 的 MOVE_RIGHT tick 写 ws.preemptive_level（1726 载荷
//     effect_move_right_priority_bonus 直系：{owner, X, +{2}}，[0]=owner/[2]=bonus）；
//   - 实例：高温晶核 31298 [1029]|[200,1,1]、五彩水域 35965 [1029]|[300,1,1]。
// 1223（args_num=3）"造成的攻击伤害若高于{0}则令自身下{1}回合所有技能先制+{2}"：
//   - 与 1029 同构、比较方向取反（> 阈值触发）；case 1223 同一行归位；
//   - "所有技能" = preemptive_level 对本回合出招整体生效（843/499 通道语义）。
//   - 实例：密集弹幕袭 34726 [1223,475]|[300,2,1,300,2]（475=低伤必暴窗，已注册
//     e_must_crit.cpp；伤害高于其阈值 300 时不触发=良性）、幽蝶起舞 34449。
// 878（args_num=2）"下{0}回合先出手则对手{1}"：
//   - 窗 = NextRounds({0}) 的 AFTER_ACTION 双侧探针（每回合自身行动结束时点判一次）；
//   - "先出手" = 探针执行时 currentState == BATTLE_FIRST_AFTER_ACTION（自身行动方
//     过滤由桶的 per-owner 执行天然保证；J12 bucket_acting_side 同源判据）；
//   - 后果 = apply_anomaly_checked(对手, {1}, 申报 chance=100——J3/J12 719 口径；
//     异常码 ≤43；持续 random_anomaly_duration）。
//   - 实例：极光电球 33775 [878]|[1,6]（1 回合窗·害怕）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1029/1223/878 零命中（不在连击次数 switch、
//   无 rule_center 票）②effect_meta 无分类牵连 ③moves_lib 全 worktree 并集无注册——
//   真缺口。切片：side_effect 表 arg_count 1029=3/1223=3/878=2 与 effect_info.args_num
//   一致；51 载体链切片自洽（[阈值,回合,先制]×2/[回合,异常码]）。
// 内核改动：skills.cpp effect_register_state 加 case 1029/1223 两行（101/422/628/
//   1256 同款归位；与并行任务错峰动 skills.cpp——J53 先例）。
// 先例：1256（AFTER_ACTION 读 final）、1726（NextRounds MOVE_RIGHT 先制窗载荷）、
//   476（J49 acting 先后手门）、J12（门探针框架）、916（申报制异常）。
// 场景回归：test/scenario/scenario_1470_threshold_prio_window.cpp（1470 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 门探针挂载（J12 同构：FIRST/SECOND 双侧、duration=1 判定归属本次用技）──
void j61_arm_gate(BattleContext* ctx, int effect_id, int self, std::vector<int> payload,
                  EffectFn probe) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, self, -1, EffectArgs(payload), probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
}

// ── 1029 探针：伤害低于阈值 → 下{1}回合先制+{2}（载荷 [owner,target,阈值,回合,先制]）──
EffectResult j61_1029_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int threshold = a.int_args[2];
    const int rounds = a.int_args[3];
    const int prio = a.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || prio == 0) {
        return EffectResult::kOk;
    }
    if (c->resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // "低于{0}"才触发（== 不算）
    }
    register_next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1029, rounds,
                                {owner, 1 - owner, prio}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 1223 探针：伤害高于阈值 → 同上（载荷同形）──
EffectResult j61_1223_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int threshold = a.int_args[2];
    const int rounds = a.int_args[3];
    const int prio = a.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || prio == 0) {
        return EffectResult::kOk;
    }
    if (c->resolvedDamage.final <= threshold) {
        return EffectResult::kOk;   // "高于{0}"才触发
    }
    register_next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1223, rounds,
                                {owner, 1 - owner, prio}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 878 窗 tick：本回合自身先出手 → 对手{1}（载荷 [owner,target,回合,异常码,武装回合]）──
EffectResult j61_878_first_strike_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int anomaly = a.int_args[3];
    if (self < 0 || self > 1 || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // 武装发生在 SKILL_EFFECT（回合 R），AFTER_ACTION 桶在同回合更晚——NextRounds
    // 起点挡不住同回合执行，这里显式跳过武装当回合（"下{0}回合"从 R+1 起）。
    if (c->roundCount <= a.int_args[4]) {
        return EffectResult::kOk;
    }
    // 自身行动方过滤由桶的 per-owner 执行保证；"先出手" = 自身行动结束发生在
    // FIRST_AFTER_ACTION（本回合先手方的收尾时点）。
    if (c->currentState != State::BATTLE_FIRST_AFTER_ACTION) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(c, 1 - self, anomaly, random_anomaly_duration(),
                                        self, /*chance=*/100, ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

// ── 1029 —— 造成的伤害低于{0}则下{1}回合自身先制+{2} ──
// int_args = [owner, target, threshold, rounds, prio]（args_num=3）。
EffectResult effect_1029_low_dmg_prio_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1029, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1029, args,
                     "1029 布局=[owner,target,threshold,rounds,prio]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1029, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[3] > 0, 1029, args, "需要 rounds>0");
    j61_arm_gate(ctx, 1029, owner,
                 std::vector<int>(args.int_args, args.int_args + args.int_count),
                 &j61_1029_probe);
    return EffectResult::kOk;
}

// ── 1223 —— 造成的攻击伤害若高于{0}则令自身下{1}回合所有技能先制+{2} ──
EffectResult effect_1223_high_dmg_prio_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1223, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1223, args,
                     "1223 布局=[owner,target,threshold,rounds,prio]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1223, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[3] > 0, 1223, args, "需要 rounds>0");
    j61_arm_gate(ctx, 1223, owner,
                 std::vector<int>(args.int_args, args.int_args + args.int_count),
                 &j61_1223_probe);
    return EffectResult::kOk;
}

// ── 878 —— 下{0}回合先出手则对手{1} ──
// int_args = [owner, target, rounds, anomaly]（args_num=2）。窗武装无条件（模板无门）。
EffectResult effect_878_first_strike_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 878, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 878, args,
                     "878 布局=[owner,target,rounds,anomaly]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     878, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 878, args, "需要 rounds>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 878, args, "异常码越界（≤43）");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(ctx, st, owner, 878, rounds,
                                    {owner, target, rounds, anomaly, ctx->roundCount},
                                    &j61_878_first_strike_tick);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1029, effect_1029_low_dmg_prio_window)
SKILL_EFFECT(1223, effect_1223_high_dmg_prio_window)
SKILL_EFFECT(878, effect_878_first_strike_window)
