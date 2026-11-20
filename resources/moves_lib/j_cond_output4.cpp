// J 组 J49 · 条件输出族四 —— 3 宏/55 载体（2026-10-02，worktree ../agents/j49）
//
// 家族语义（effect_info 模板即权威）：
//   814 [1] 先出手时附加自身双防值总和{0}%的百分比伤害
//   415 [2] 若造成的伤害大于{0}点，则自身恢复{1}点生命值
//   476 [1] 后出手时恢复{0}点体力
//
// 实现口径：
//   - 814 = 943（视图双防和粉，676 口径）+ 664 先手门（acting_side_is_first，
//     980/52 判据）。单刀直附（1460 同款：进 SKILL_EFFECT 桶=已命中）。
//   - 415 = J40 1522 的 heal 版：DETECT 攻方桶读本次红伤 final **严格 >** {0} →
//     heal_amount({1})（heal 钳满）。
//   - 476 = 969 后手门取反（acting_side_is_first == false）→ heal_amount({0})。
//
// 载体切片（unity 库 json_each 精确匹配 18+18+19=55；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   814  [pct]
//   415  [阈值,回量]
//   476  [点数]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_overrides 无声明
// ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1350_cond_output4.cpp（号段 1350-1359）。

#include "common.h"

namespace {

// ── 814 —— 先出手时附加自身双防值总和{0}%的百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_814_first_def_sum_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 814, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 814, args,
                     "814 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     814, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 814, args, "需要 pct > 0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门（后手无事发生）
    }
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int def_sum = ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE)
        + ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE);
    const int amount = def_sum * pct / 100;
    if (amount > 0 && ctx->getPet(target).hp > 0) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 415 —— 若造成的伤害大于{0}点，则自身恢复{1}点生命值 ──
// int_args = [owner, target, threshold, heal]（args_num=2）。
EffectResult effect_415_high_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 415, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 415, args,
                     "415 布局=[owner,target,阈值,回量]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int heal = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     415, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && heal > 0, 415, args, "需要 阈值/回量 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, heal](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤附加
            }
            if (d.final <= threshold) {
                return;   // "造成的伤害大于{0}"（严格大于）
            }
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(c, owner, heal);
            }
        });
    return EffectResult::kOk;
}

// ── 476 —— 后出手时恢复{0}点体力 ──
// int_args = [owner, target, points]（args_num=1）。
EffectResult effect_476_second_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 476, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 476, args,
                     "476 布局=[owner,target,点数]");
    const int self = args.int_args[0];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     476, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(points > 0, 476, args, "需要 点数 > 0");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门：先手无事发生
    }
    if (g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, self, points);   // heal 钳满
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(814, effect_814_first_def_sum_pink)
SKILL_EFFECT(415, effect_415_high_damage_heal)
SKILL_EFFECT(476, effect_476_second_heal)
