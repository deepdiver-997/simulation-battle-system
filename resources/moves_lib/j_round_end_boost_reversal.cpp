// J 组 J222 · 回合末反转提升窗族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1566 [1] {0}回合内每回合结束后反转对手能力提升状态
//
// 实现口径（直系先例）：
//   - 窗 = register_round_effect(BATTLE_ROUND_END, InRounds, {0})（759 同 helper 口径：
//     先手方当回合末即结算；后手方 N>=2 由 round_effect_start_round 顺延一回合）。
//   - tick = stat_boost_reversal(对手)（978/970 同原语；弱化类，查 STAT_DROP 免弱，
//     正等级 → 等量负等级；其余槽不动）。"反转"不是"消除"——无提升/被免弱时自然空跑。
//   - 行参在位即消费：{0} 直读，不设默认窗长；显式 0/负档 = 不注册窗口（不回退）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 9/9 自洽）：1566 [rounds]：28303 邪翅送葬 [3]（纯链场景载体）/ 28545 [3] / 28551 [3] /
//   28365 [2] / 27124 [3] / 28996 [3] / 29337 [3] / 36490 [3] / 37261 [3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1566 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5520_round_end_boost_reversal.cpp（号段 5520-5529）。

#include "common.h"

namespace {

// ── 1566 ROUND_END tick：反转对手全部正等级 ──
EffectResult j222_1566_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->stat_boost_reversal || !args.int_args
        || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_boost_reversal(ctx, opp);
    return EffectResult::kOk;
}

// ── 1566 —— {0}回合内每回合结束后反转对手能力提升状态 ──
// int_args = [owner, target, rounds]（args_num=1 → 装载器注入 owner/target）。
EffectResult effect_1566_each_round_end_reverse_boosts(BattleContext* ctx,
                                                       const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1566, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1566, args,
                     "1566 布局=[owner,target,rounds]（args_num=1）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1566,
                     args, "需要 owner/target ∈ {0,1}");
    // {0} 在位即消费：0/负档 = 不注册窗口。
    if (rounds > 0) {
        register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1566, rounds, {owner, opp},
                              &j222_1566_round_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1566, effect_1566_each_round_end_reverse_boosts)
