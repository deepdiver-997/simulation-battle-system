// J 组 J213 · 吸强兜底吸血族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1928 [1] 吸取对手能力提升状态，若对手不处于能力提升状态则吸取对手最大体力的1/{0}
//
// 实现口径（J120 1127 同门，语序判据直系）：
//   - 吸取 = transfer_stat_boosts(opp→self)（85/1287/1062 共用原语；被免消除强化挡下返回 0）。
//   - 条件子句按 1127 已定版口径**在吸取之后判**"对手不处于能力提升状态"（J120 记档：
//     "吸取成功后对手正等级已整体搬到自身 → 条件恒真，官方文本语序如此"）。本族不另立
//     新语义——同形模板必须走同一判据，否则 1127/1928 两族行为分叉。
//     ⇒ 实际效果：吸取照常；只要不是"有强化但被免消除挡下"，兜底吸血都落。
//   - 兜底 = drain_hp(owner, opp, {0})：内核读对手 numericalBase[HP] 算 max_hp/{0}，
//     目标吃 FIXED 固定伤害、自身 heal_impl 回等量（1062 同通道；不手搓双原语）。
//   - {0} 在位即消费：分母必须 >0（0 = 非法分母，不是可关闸的 pct 档，此处不适用红线7
//     的"显式 0"语义——模板是 1/{0} 的除数）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 9/9 自洽）：
//   1928 [分母]：36423 神州烈阳舞 [3]、36440 启·辉霆璨世 [3]、36656 鹏·万里遨摇 [4]、
//     36739 黑胧 [3]、37026 响尾 [3]、37846 悬河 [3]、38109 赤炎燎原 [3]、
//     38320 能力汲取 [3]…（全载体 1/3 或 1/4）
//
// 动工前置三查（2026-10-22）：①内核 grep 1928 无硬编码 ②custom_effect_programs/
// overrides 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5400_drain_boost_fallback.cpp（号段 5400-5409）。

#include "common.h"

namespace {

// "对手不处于能力提升状态"（六槽无正等级）。
inline bool j213_has_no_stat_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

// ── 1928 —— 吸取对手能力提升状态，若对手不处于能力提升状态则吸取对手最大体力的1/{0} ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1928_drain_boost_or_drain_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1928, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1928, args,
                     "1928 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1928, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1928, args, "需要 分母 > 0");
    if (!g_core || !g_core->transfer_stat_boosts || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    (void)g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner);
    // 1127 同门判据：吸取后判"对手不处于能力提升状态"（正等级已搬到自身 → 条件恒真）。
    if (j213_has_no_stat_boost(ctx, opp)) {
        (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, denom);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1928, effect_1928_drain_boost_or_drain_hp)
