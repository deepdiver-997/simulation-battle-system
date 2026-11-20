// J 组 J165 · 申报兜底族二 —— 1000×12 / 1201×12（2026-10-04，worktree ../agents/j165）
//
// 家族语义（effect_info 官方模板即权威）：
//   1000 [3] 命中则{0}%使对手{1}，未触发则附加{2}点固定伤害
//   1201 [4] 先出手时{0}%使对手{1}，未触发则下{2}回合自身先制+{3}
//
// 实现口径：
//   - 1000 = SKILL_EFFECT：roll {0}% → apply_anomaly_checked；未落地（anomaly_landed
//     全档=SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED，J4 判据）→ fixed_damage({2})
//     （J107 1003 直系换兜底；"命中则"门=ws.skill_exec_result==HIT，miss 全跳——J23 36 口径）。
//   - 1201 = SKILL_EFFECT（执行期 preemptive_right 已定）：先手门（preemptive_right
//     归我）→ roll {0}% → apply_anomaly_checked；未落地 → register_next_rounds_effect
//     (**FIRST_MOVE_RIGHT**, {2} 回合) preemptive+{3}（J115/J133 正位通道）。非先手
//     → 全跳（字面门；"未触发"是否含"非先手"两读，按窄义记档）。
//
// 载体切片（unity 库 json_each 精确匹配 12+12=24；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   1000 [概率,码,固伤]：{80,9,250}/{60,6,250}/{50,15,200}/… 纯链×4——场景载体
//     遮魂裂 32975 [50,15,200] 物 pri0（码 15；38360 码 0 非确定坑避用——J97 记档）
//   1201 [概率,码,回合,先制]：{100,0,2,1}/{70,15,2,1}/{80,6,2,1}/… 纯链×3——场景
//     载体 乘风化龙 32137 [70,15,2,1] 特 pri0（码 15 确定档）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 2 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：J107 1003（申报制+未触发兜底）、J23 36（命中门）、J4 anomaly_landed、
//   J115/J133（NextRounds FIRST_MOVE_RIGHT 先制 tick）、629（apply_anomaly_checked 形）。
// 场景回归：test/scenario/scenario_2640_prob_fallback4.cpp（2640 号段）。

#include "common.h"

namespace {

bool j165_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 1000 —— 命中则{0}%使对手{1}，未触发则附加{2}点固定伤害 ──
// int_args = [owner, target, chance, anomaly, fixed]（args_num=3）。
EffectResult effect_1000_prob_or_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1000, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1000, args,
                     "1000 布局=[owner,target,概率,异常码,固伤]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int fixed = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1000, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && fixed > 0, 1000, args, "需要 概率/固伤 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1000, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中则"门（miss/无效全跳）
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (!j165_anomaly_landed(r) && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, fixed);   // 未触发兜底
    }
    return EffectResult::kOk;
}

// ── 1201 —— 先出手时{0}%使对手{1}，未触发则下{2}回合自身先制+{3} ──
// int_args = [owner, target, chance, anomaly, rounds, prio]（args_num=4）。
EffectResult effect_1201_first_prob_or_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1201, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1201, args,
                     "1201 布局=[owner,target,概率,异常码,回合,先制]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    const int prio = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1201, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && rounds > 0 && prio > 0, 1201, args,
                     "需要 概率/回合/先制 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1201, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 先手门：执行期 preemptive_right 已结算。
    const bool self_first =
        (ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_1 && owner == 0)
        || (ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 && owner == 1);
    if (!self_first) {
        return EffectResult::kOk;   // 非先手全跳（窄义记档）
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (!j165_anomaly_landed(r)) {
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1201,
                                    rounds, {owner, 1 - owner, prio},
                                    &effect_move_right_priority_bonus);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1000, effect_1000_prob_or_fixed)
SKILL_EFFECT(1201, effect_1201_first_prob_or_prio)
