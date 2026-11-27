// J90 · 自身强化翻倍族 —— 622×17（2026-10-03，worktree ../agents/j90）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 622（args_num=7）"自身{0}，{6}%概率强化效果翻倍"：
//   - {0} = 语义序六槽向量（攻,防,特攻,特防,速,命中 → kStatMap {0,2,1,3,4,5}，J48
//     同表）；载体实载全正值（寒来暑往 [1,1,1,1,1,0,20]）→ 自身提升 stat_change
//     （不吃免弱，common.h effect 4 口径；负值容错同原语=自身下降）；
//   - "强化效果翻倍" = roll_percent({6}) → 逐**正槽**再 stat_change(+当前等级)
//     （J12 964 翻倍取法：+1 → +2，AT_CAP 自然封顶；掷歪则维持首段）。
//   - 实例：寒来暑往 26696 [622]|[1,1,1,1,1,0,20]（属性技纯链）。
//
// 动工前置三查（2026-10-03）：①内核 grep 622 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   622=7 与 effect_info.args_num 一致；17 载体切片自洽（版式=[六槽×6, chance]
//   收尾——新月祝福/驭鹰神力对齐核验）。
// 先例：J48（kStatMap）、J12 964（翻倍取法）、J54（ 六槽向量版式经验）。
// 场景回归：test/scenario/scenario_1790_self_boost_double.cpp（1790 号段）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（J3/J48 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

}  // namespace

// ── 622 —— 自身{0}（语义序六槽），{6}%概率强化效果翻倍 ──
// int_args = [owner, target, 六槽×6, chance]（args_num=7）。
EffectResult effect_622_self_boost_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 622, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 622, args,
                     "622 布局=[owner,target,六槽×6,chance]");
    const int self = args.int_args[0];
    const int chance = args.int_args[8];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 622, args, "owner 越界");
    EFFECT_ARG_GUARD(chance > 0, 622, args, "需要 chance>0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    // 首段：六槽逐项自身变化（实载全正值=提升）。
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        if (delta != 0) {
            (void)g_core->stat_change(ctx, self, kStatMap[i], delta);
        }
    }
    // 翻倍段：掷点命中 → 逐正槽再 +当前等级（964 取法，AT_CAP 自然封顶）。
    if (roll_percent(ctx->rng(), chance)) {
        for (int i = 0; i < 6; ++i) {
            const int lv = ctx->ability_levels[self][kStatMap[i]];
            if (lv > 0) {
                (void)g_core->stat_change(ctx, self, kStatMap[i], lv);
            }
        }
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(622, effect_622_self_boost_double)
