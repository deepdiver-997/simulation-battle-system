// J 组 J215 · 无提升门异常族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   2004 [2] 若对手不处于能力提升状态则{0}%令对手{1}
//
// 实现口径（条件门 + 申报制异常，J3/J12 同型）：
//   - 门 = "对手不处于能力提升状态"（六槽无正等级；J120 同判据）——在 SKILL_EFFECT
//     时点同步查询（技能链内前序子句已结算完，读到的就是本次命中时状态）。
//   - 概率走 apply_anomaly_checked（申报 {0}%，受亮节闸门管辖）；{0}=0 在位即消费
//     （红线7：0 是显式关闸档，不是非法参数）。
//   - 异常时长 random_anomaly_duration()（当前恒 2，与既有施加族一致）。
//   - 条件不成立 → 整条无事（不申请、不掷点）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   2004 [概率, 异常码]：36886 光刃痛击 [100,20]（纯链）、36658 喵喵笔记 [100,15]、
//     37328 聚光魔法 [70,8]、37833 水花迸射 [50,5]、38307 返祖撼击 [80,6]…（8 载体）
//
// 动工前置三查（2026-10-22）：①内核 grep 2004 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5450_anomaly_gate_no_boost.cpp（号段 5450-5459）。

#include "common.h"

namespace {

// "对手不处于能力提升状态"（六槽无正等级；J120/J213 同判据）。
inline bool j215_has_no_stat_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

// ── 2004 —— 若对手不处于能力提升状态则{0}%令对手{1} ──
// int_args = [owner, target, 概率, 异常码]（args_num=2）。
EffectResult effect_2004_no_boost_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2004, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2004, args,
                     "2004 布局=[owner,target,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     2004, args, "需要 owner/target ∈ {0,1}");
    // 红线7：概率在位即消费（0 = 显式关闸档，仍申请、掷点必败——不得当非法参数）。
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 2004, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2004, args, "需要 异常码 ∈ [0,43]");
    if (!j215_has_no_stat_boost(ctx, target)) {
        return EffectResult::kOk;   // 门闭：对手有正等级
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(), owner,
                                        chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2004, effect_2004_no_boost_anomaly)
