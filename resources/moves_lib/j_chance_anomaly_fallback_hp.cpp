// J 组 J209 · 概率异常未触发→当前体力比例粉伤族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   974 [3] {0}%使对手{1}，若未触发则使对手受到相当于自身当前体力1/{2}的百分比伤害
//
// 实现口径：
//   - apply_anomaly_checked(opp, {1}, random_anomaly_duration(), self, {0}%,
//     ChanceSource::Skill)——概率申报给原语（F7）；返回 ROLL_FAILED **才是**"未触发"
//     （被免疫/抗性/弹控是另一档，不走兜底——ApplyAnomalyResult 语义）。
//   - 兜底 = deal_pink_damage(opp, 自身**当前**体力/{2}, PERCENT_VALUE)（1136/1137
//     体力比例粉伤通道；SKILL_EFFECT 时点读到的体力是本刀伤害前）。
//   - 审计槽：990974100+self = ApplyAnomalyResult 整数值；990974101+self = 兜底粉伤值。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   974 [概率,异常,分母]：34302 睡意环绕 纯链 [50,8,3]（特 130，场景载体）、
//        27693 赤武之心 [100,2,2]、37257 血引斩魂剑 [100,16,3]、24967 死寂黎明 [80,8,2]…
//        （9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 974 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(974)——真缺口。
// 场景回归：test/scenario/scenario_5350_chance_anomaly_fallback_hp.cpp（号段 5350-5359）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k974ResultBase = 990974100;    // +self*2：ApplyAnomalyResult 整数
constexpr int k974FallbackBase = 990974101;  // +self*2：兜底粉伤值

// ── 974 —— {0}%使对手{1}，未触发则对手受到自身当前体力1/{2}的百分比伤害 ──
// int_args = [owner, target, 概率, 异常, 分母]（args_num=3）。
EffectResult effect_974_chance_anomaly_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 974, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 974, args,
                     "974 布局=[owner,target,概率,异常,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     974, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100 && denom > 0, 974, args,
                     "需要 概率∈(0,100] 且 分母>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 974, args, "需要合法异常 id");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, pct, ChanceSource::Skill);
    ctx->plugin_storage[k974ResultBase + self * 2] = static_cast<int>(r);
    if (r != ApplyAnomalyResult::ROLL_FAILED) {
        return EffectResult::kOk;   // 已触发/被免疫/被抗性 → 无兜底
    }
    const int amount = ctx->getPet(self).hp / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k974FallbackBase + self * 2] = amount;
    if (g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(974, effect_974_chance_anomaly_fallback)
