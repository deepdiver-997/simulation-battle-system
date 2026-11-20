// J 组 J35 · 条件粉伤/条件增伤族 —— 3 宏/60 载体（2026-10-02，worktree ../agents/j35）
//
// 家族语义（effect_info 模板即权威）：单刀条件 rider——伤害/粉伤按条件直附。
//   1046 [1] 若对手不处于异常状态则附加对手最大体力1/{0}的百分比伤害
//   1198 [1] 对手不处于能力下降状态则造成的伤害提高{0}%
//   943  [1] 附加自身防御、特防总和{0}%的百分比伤害
//
// 实现口径：
//   - 1046 = SKILL_EFFECT 直附：!has_any_active_abnormal_status(target)（J14 助手取反）
//     → deal_pink_damage(target, max_hp_of(target)/{0}, PERCENT_VALUE)（1255 口径：
//     max_hp 读 numericalBase）。
//   - 1198 = 1439 镜像条件（J17 同形）：!has_any_stat_drop(target)（任一负槽判据取反）
//     → ws.damage_add_pct[owner] += {0}——直写落**本次攻击** snapshot（1037 通道）。
//   - 943 = 676 无窗口版：deal_pink_damage(target, 视图双防和×{0}/100, PERCENT_VALUE)；
//     双防读 **视图槽** getTempAbilityValue（2501/676 同口径，面板改一行记档）。
//   通道统一 PERCENT_VALUE（模板"百分比伤害"，计算值粉伤 1231/1875 同通道）。
//
// 载体切片（unity 库 json_each 精确匹配 21+20+19=60；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   1046 [分母]：19989 王·天蛇决意 [3]（特160）
//   1198 [pct]：30895 妃·灾厄黎明 [50]（特160）
//   943  [pct]：19615 狂野搏击 [30]（物85）
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1200_cond_pink_amp.cpp（号段 1200-1209）。

#include "common.h"

namespace {

// 对手处于能力下降状态（任一负槽，六槽含命中——J14/J17 同判据）。
inline bool j35_has_any_stat_drop(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1046 —— 若对手不处于异常状态则附加对手最大体力1/{0}的百分比伤害 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_1046_no_anomaly_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1046, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1046, args,
                     "1046 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1046, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1046, args, "需要 分母 > 0");
    if (ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 门槛：对手不处于异常（取反判据）
    }
    if (g_core && g_core->deal_pink_damage) {
        const int amount = max_hp_of(ctx, target) / denom;
        if (amount > 0 && ctx->getPet(target).hp > 0) {
            (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE,
                                           owner);
        }
    }
    return EffectResult::kOk;
}

// ── 1198 —— 对手不处于能力下降状态则造成的伤害提高{0}% ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_1198_no_drop_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1198, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1198, args,
                     "1198 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1198, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 1198, args, "需要 pct > 0");
    if (j35_has_any_stat_drop(ctx, target)) {
        return EffectResult::kOk;   // 门槛取反：对手无下降才提升
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(pct);   // 落本次攻击 snapshot（1439 形）
    return EffectResult::kOk;
}

// ── 943 —— 附加自身防御、特防总和{0}%的百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_943_def_sum_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 943, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 943, args,
                     "943 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     943, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 943, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 视图双防和（676 同口径）。
    const int def_sum = ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE)
        + ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE);
    const int amount = def_sum * pct / 100;
    if (amount > 0 && ctx->getPet(target).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1046, effect_1046_no_anomaly_pink)
SKILL_EFFECT(1198, effect_1198_no_drop_amp)
SKILL_EFFECT(943, effect_943_def_sum_pink)
