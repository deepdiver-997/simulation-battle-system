// J 组 J241 · 降态增伤族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   858（args_num=1）"造成的伤害提升，伤害提升的倍数等于对手能力下降等级总和乘以{0}%"
//
// 实现口径：
//   - 求和 = 对手 6 个能力槽的**负等级绝对值**之和（读 `ws.view_levels`，与伤害公式
//     同源；"下降等级总和"= 各槽 -level 相加，非种类数——713/1772 的求和判据）。
//   - 落点 = `ws.damage_add_pct[owner] += sum × {0}`（通用增伤加算通道，1439/890 同款；
//     SKILL_EFFECT 时点写入，AMP 阶段读取）。
//   - `{0}` 在位即消费：<=0 显式关闸（不回落默认值；本宏全库实载 10/15/20）。
//   - 对手无负等级 → sum=0 → 纯 no-op（不加 0 档）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   **19048 噬神叹息斩 [180,858]|[10]（物 160；180=清回合类效果，对手无回合效果时
//     no-op——场景载体）** / 30157 [1070,858]|[10] / 31112 [1174,858]|[10] /
//   32329 [1234,679,858]|[1,1,20] / 32679 [858,731,840]|[15,1,3] /
//   33199 [1180,1107,858]|[2,50,1,50,1,10] / 35105 [1124,938,858]|[1,100,1,10] /
//   36021 [1750,494,1568,942,858]|[3,20]。
//
// 动工前置三查（2026-10-22）：①内核 grep 858 无硬编码（view_levels/damage_add_pct 是
//   通用设施）②effect_info 仅本条模板 ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/
//   j198/k347 各 worktree + 全部本地分支）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5720_negative_level_amp.cpp（号段 5720-5729）。

#include "common.h"

namespace {

// ── 858 —— 伤害提升倍数 = 对手能力下降等级总和 × {0}% ──
EffectResult effect_858_negative_level_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 858, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 858, args,
                     "858 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 858,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 不增伤（不回退默认值）。
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int level = ctx->ws.view_levels[opp][i];
        if (level < 0) {
            sum += -level;   // 下降等级总和（绝对值）
        }
    }
    if (sum <= 0) {
        return EffectResult::kOk;   // 对手无下降状态
    }
    ctx->ws.damage_add_pct[owner] += static_cast<float>(sum * pct);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(858, effect_858_negative_level_amp)
