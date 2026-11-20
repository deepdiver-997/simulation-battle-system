// J 组 J128 · 门族三（异常）—— 434×13 / 2158×12（2026-10-04，worktree ../agents/j128）
//
// 家族语义（effect_info 模板即权威）：
//   434 （args_num=2）"若自身处于能力提升状态则{0}%概率令对手{1}"
//     —— 冰冻之光 13913 [100,5]（纯链特80，100% 冻伤确定档）。
//   2158（args_num=1）"解除自身所处的异常状态，解除成功令对手{0}"
//     —— 星光·惊涛骇浪 37386 [15]（纯链特140，成功→对手冰封；板行 [9] 系速记笔误，
//        以切片为准）。
//
// 实现口径：
//   - 434 = 自身任一正槽门（count_boost_kinds 同式判据：六槽含命中、负槽不算——
//     J2 1361/f5 助手同款）→ apply_anomaly_checked(opp, {0}%, 码{1},
//     random_anomaly_duration())（J3 申报制直系）。门不成立先短路后掷骰。
//   - 2158 = dispel_active_anomalies(自身) 返回**解除条数**（星启 4677 同款 CoreApi 槽，
//     效果解除路径：直接清槽、无伤害无事件）→ 条数>0 成功门 → apply_anomaly_checked
//     (opp, 100%, 码{0})（J110 2196 申报变体）。无异常可解 = 解除失败 = 无事发生
//     （不耗 rand——门在掷骰前）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   434  [概率,异常码]：13913 冰冻之光 [100,5]、34473 泡沫炸弹 [100,5]、
//        13599 海啸冲击破 [50,5]、14954 火焰路径 [50,2]、15223 幻影暴风诀 [50,7]…
//        （13/13 自洽，纯链×5）
//   2158 [异常码]：37386 星光·惊涛骇浪 [15]、37869 圣光碰撞 [7]、37923 集中散热 [22]
//        （12/12 自洽，纯链×3）
//
// 动工前置三查（2026-10-04）：①内核 grep 434/2158 无行为硬编码 ②effect_meta 无分类
// 牵连 ③moves_lib 无既有注册——真缺口（dispel_active_anomalies/apply_anomaly_checked
// 均既有设施；含 CARRY_EFFECT 复核）。
// 场景回归：test/scenario/scenario_2220_anomaly_gates3.cpp（号段 2220-2229）。

#include "common.h"

namespace {

// 自身任一正槽（六槽含命中，负槽不算——J2 1361/f5 同判据）。
bool j128_has_any_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 434 —— 若自身处于能力提升状态则{0}%概率令对手{1} ──
// int_args = [owner, target, 概率, 异常码]（args_num=2）。
EffectResult effect_434_boost_cond_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 434, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 434, args,
                     "434 布局=[owner,target,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     434, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0, 434, args, "需要 概率 > 0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (!j128_has_any_boost(ctx, owner)) {
        return EffectResult::kOk;   // 门不成立：先短路后掷骰
    }
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(), owner,
                                        chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 2158 —— 解除自身所处的异常状态，解除成功令对手{0} ──
// int_args = [owner, target, 异常码]（args_num=1）。
EffectResult effect_2158_dispel_cond_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2158, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2158, args,
                     "2158 布局=[owner,target,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2158, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->dispel_active_anomalies || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    if (g_core->dispel_active_anomalies(ctx, owner) <= 0) {
        return EffectResult::kOk;   // 解除失败（自身无异常）：无事发生、不耗 rand
    }
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(), owner,
                                        /*chance_pct=*/100, ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(434, effect_434_boost_cond_anomaly)
SKILL_EFFECT(2158, effect_2158_dispel_cond_anomaly)
