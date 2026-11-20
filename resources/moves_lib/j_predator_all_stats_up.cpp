// J 组 J250 · 天敌全属性强化族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   2057（args_num=1）"全属性+{0}，自身为对手天敌时强化效果翻倍"
//
// 实现口径：
//   - 天敌判定 = `is_predator(ctx, owner)`（common.h；官方实测口径：**对手的固有属性
//     克制自身固有属性**，双属性按克制表相乘；不看技能属性）。
//   - 落点 = `all_stats_up(自身, n)`，n = {0}×2（天敌）或 {0}（否则）——六项含命中，
//     走 stat_change 真原语（自身增益不吃免弱，本体/视图同步，±6 钳制）。
//   - `{0}` 在位即消费：<=0 = 不强化（不回退默认值）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   28394 [2057,43,843]|[1,1,2,1] / 28396 [2057,1005,1731,843]|[1,1,3,300,2,2,2] /
//   28505 [2057,1842,153,843]|[1,4,300,450,4,1,2,2] / 28753 [1030,2057,597,1567]|[15,1,4,3] /
//   29011 [2058,2057,1647,2284,843]|[1,4,3,2,250,2,250,2,2] /
//   29162 [2057,971,843]|[1,4,3,2,2,2] /
//   **29290 活化姿态 [2057,843]|[1,2,2]（纯链旁证，场景载体；843=先制不碰等级）** /
//   29387 [2057,2320,776,843]|[1,2,0,0,0,0,-2,0,1,2,2,2]。
//   全库实载 {0} ∈ {1,4}。
//
// 动工前置三查（2026-10-22）：①内核 grep 2057 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5810_predator_all_stats_up.cpp（号段 5810-5819）。

#include "common.h"

namespace {

// ── 2057 —— 全属性+{0}，自身为对手天敌时强化效果翻倍 ──
EffectResult effect_2057_predator_all_stats_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2057, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2057, args,
                     "2057 布局=[owner,target,amount]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 2057,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 不强化（不回退默认值）。
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    const int delta = is_predator(ctx, owner) ? amount * 2 : amount;
    all_stats_up(ctx, owner, delta);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2057, effect_2057_predator_all_stats_up)
