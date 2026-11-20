// J 组 J210 · 体力优势门概率异常→未触发比例粉伤族 —— 1 宏/10 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   1570 [3] 出手时若自身体力高于对手则{0}%令对手{1}，未触发则附加对手最大体力1/{2}的百分比伤害
//
// 实现口径：
//   - 门 = 出手时（SKILL_EFFECT）自身当前体力**严格大于**对手（阅读"高于"；相等不触发）。
//   - 概率异常走 apply_anomaly_checked(opp, {1}, random_anomaly_duration(), self, {0}%,
//     ChanceSource::Skill)；只有 ROLL_FAILED（未触发）走兜底——被免疫/抗性不是"未触发"。
//   - 兜底 = deal_pink_damage(opp, max_hp(opp)/{2}, PERCENT_VALUE)（1136/1137 同通道）。
//   - 审计槽：991570000+self = ApplyAnomalyResult；991570001+self = 兜底粉伤值。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1570 [概率,异常,分母]：38288 重装践踏 链 [1570,485] 段 [70,7,4]（物 85，场景载体；
//        485 消强成功才回血，对手无提升 → 静默）、34320 [50,19,3]、35519 [60,27,3]、
//        34115 [80,15,3]…（10/10 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1570 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(1570)——真缺口。
// 场景回归：test/scenario/scenario_5360_high_hp_gate_anomaly_fallback.cpp（号段 5360-5369）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1570ResultBase = 991570000;    // +self*2：ApplyAnomalyResult
constexpr int k1570FallbackBase = 991570001;  // +self*2：兜底粉伤值

// ── 1570 —— 出手时自身体力高于对手则{0}%令对手{1}，未触发则粉伤 max/{2} ──
// int_args = [owner, target, 概率, 异常, 分母]（args_num=3）。
EffectResult effect_1570_high_hp_gate_anomaly_fallback(BattleContext* ctx,
                                                       const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1570, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1570, args,
                     "1570 布局=[owner,target,概率,异常,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1570, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100 && denom > 0, 1570, args,
                     "需要 概率∈(0,100] 且 分母>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1570, args, "需要合法异常 id");
    if (ctx->getPet(self).hp <= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // 门：出手时自身体力严格高于对手
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, pct, ChanceSource::Skill);
    ctx->plugin_storage[k1570ResultBase + self * 2] = static_cast<int>(r);
    if (r != ApplyAnomalyResult::ROLL_FAILED) {
        return EffectResult::kOk;
    }
    const int amount = max_hp_of(ctx, opp) / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1570FallbackBase + self * 2] = amount;
    if (g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1570, effect_1570_high_hp_gate_anomaly_fallback)
