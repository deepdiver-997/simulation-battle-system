// J 组 J120 · 吸强兜底全属性降族 —— 1127×13（2026-10-03，worktree ../agents/j120）
//
// 家族语义（effect_info 官方模板即权威）：
//   1127（arg_count=1）"吸取对手能力提升状态，若对手不处于能力提升状态则使对手全属性-{0}"
//
// 实现口径（J4 吸强家族补员——1159/1022/1261 已落地，本条是同门单宏）：
//   - 吸取 = transfer_stat_boosts(opp→self)（85 转化/1287 吸取共用原语；被免消除
//     强化挡下返回 0）；
//   - 条件子句 = has_no_stat_boost(opp) 显式判（J4 记档沿用：吸取成功后对手正等级
//     已整体搬到自身 → 条件恒真，官方文本语序如此）→ all_stats_down 六槽 stat_drop
//     {0}（1009 同款）。两分支最终都落"对手全属性-{0}"——差异在强化是否归我。
//   - 本文件自带 drain_succeeded/has_no_stat_boost/all_stats_down 本地副本
//     （j_drain_boost_success.cpp 同款，不跨文件共享插件内部助手）。
//
// 载体切片（unity 库 json_each 精确匹配 13/13；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-03）：1127 [降幅] ×13 全 [1]（六槽各 -1）。
// 无纯链——场景载体 冥域妖极天 30621 [1127,999]|[1,1,10] 物160 prio0 链最短
// （999 未实现哑；⚠️ 链激活风险已对冲：场景断言走能力等级面，不碰伤害数值）。
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1127 无硬编码 case
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：j_drain_boost_success（J4：drain_succeeded/has_no_stat_boost/all_stats_down、
//   1542/1726 条件子句同构）、transfer_stat_boosts（CoreApi 槽）。
// 场景回归：test/scenario/scenario_2140_drain_fallback_alldown.cpp（2140 号段）。

#include "common.h"

namespace {

// 吸取成功 = 对手正等级整体搬到自身（返回搬走项数 >0）。
inline bool j120_drain_succeeded(BattleContext* ctx, int owner, int opp) {
    return g_core && g_core->transfer_stat_boosts
        && g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) > 0;
}

// "对手不处于能力提升状态"（六槽无正等级；吸取成功后恒真——J4 口径）。
inline bool j120_has_no_stat_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

// 条件"全属性-{n}"（六槽 stat_drop，弱化原语查免弱；1009 同款）。
void j120_all_stats_down(BattleContext* ctx, int owner, int opp, int n) {
    if (!g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, n, owner);
    }
}

// ── 1127 —— 吸取对手能力提升状态，若对手不处于能力提升状态则使对手全属性-{0} ──
// int_args = [owner, target, all_down]（args_num=1）。
EffectResult effect_1127_drain_or_alldown(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1127, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1127, args,
                     "1127 布局=[owner,target,全属性降幅]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int all_down = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1127, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(all_down > 0, 1127, args, "需要 全属性降幅 > 0");
    (void)j120_drain_succeeded(ctx, owner, opp);   // 吸取本体（成功=强化归我）
    if (j120_has_no_stat_boost(ctx, opp)) {
        j120_all_stats_down(ctx, owner, opp, all_down);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1127, effect_1127_drain_or_alldown)
