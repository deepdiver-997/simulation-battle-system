// J 组 J118 · 升降槽计数固伤族 —— 713×13 / 1772×12（2026-10-03，worktree ../agents/j118）
//
// 家族语义（effect_info 官方模板即权威）：
//   713（arg_count=1）"附加自身能力提升等级总和X{0}的固定伤害"
//   1772（arg_count=1）"附加对手能力下降等级总和X{0}的固定伤害"
//
// 实现口径：
//   - 与 J96 542 同门但聚合不同：542 是**种类计数**（每处于一种+{0}），本族是
//     **等级求和**（Σmax(0,±level)×{0}——提升/下降跨槽累加，含命中槽，与 542 的
//     六槽口径一致）。SKILL_EFFECT 直通（542 同款）：求和 > 0 → fixed_damage(opp, Σ×{0})。
//   - 713 读自身正槽和；1772 读对手负槽和（取绝对值）。无正/负槽 → 不附加。
//
// 载体切片（unity 库 json_each 精确匹配 13+12=25；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   713 [固伤/级]：{50}×3/{60}/{40}/{30}×3/{15}×3/{10}/{20}——纯链 潮腾雾升 34036 [60] 特130
//   1772 [固伤/级]：{50}×4/{30}×4/{60}×2/{15}×2——无纯链；场景载体 凌风逸天宵 35814
//     [1243✓,191,1439,1772]|[3,50,50]（1243 已实现=先制+1 不扰伤害带；191 免反窗无异常
//     不发；1439 未实现哑——⚠️ 链激活风险已用宽断言带对冲）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 713/1772 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：542（负槽种类计数固伤，J96 直系）、808（正槽计数固伤，J27）、
//   fixed_damage/heal_amount（CoreApi 槽）。
// 场景回归：test/scenario/scenario_2120_sum_slot_fixed.cpp（2120 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 713 —— 附加自身能力提升等级总和X{0}的固定伤害 ──
// int_args = [owner, target, per_level]（args_num=1）。
EffectResult effect_713_boost_sum_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 713, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 713, args,
                     "713 布局=[owner,target,每级固伤]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int per_level = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     713, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(per_level > 0, 713, args, "需要 每级固伤 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            sum += ctx->ability_levels[owner][i];   // 提升等级求和（下降不计）
        }
    }
    if (sum > 0) {
        (void)g_core->fixed_damage(ctx, opp, sum * per_level);
    }
    return EffectResult::kOk;
}

// ── 1772 —— 附加对手能力下降等级总和X{0}的固定伤害 ──
// int_args = [owner, target, per_level]（args_num=1）。
EffectResult effect_1772_drop_sum_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1772, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1772, args,
                     "1772 布局=[owner,target,每级固伤]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int per_level = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1772, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(per_level > 0, 1772, args, "需要 每级固伤 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            sum -= ctx->ability_levels[opp][i];   // 对手下降等级求和（提升不计）
        }
    }
    if (sum > 0) {
        (void)g_core->fixed_damage(ctx, opp, sum * per_level);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(713, effect_713_boost_sum_fixed)
SKILL_EFFECT(1772, effect_1772_drop_sum_fixed)
