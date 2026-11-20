// J 组 J232 · 回合消强窗族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1042 [1] {0}回合内每回合自身技能消除对手能力提升状态
//
// 实现口径（先例 j_purge_roundly.cpp 759：ROUND_START 窗 tick + clear_stat_boosts）：
//   - 注册面窗界：register_round_effect(BATTLE_ROUND_START, owner, 1042, {0} 回合)
//     —— 轮次边界/过期/离场清锚全自动（T6 注册面窗界口径），不自记账本。
//   - 当回合计入："N回合内"由 round_effect_start_round 折算：先手 start==roundCount
//     → 立即消除一次（ROUND_START 本回合已过）；后手 N>=2 顺延到 R+1、不立即；
//     N==1 两者都 start==roundCount → 立即（窗即本回合）。与 759 的 1:1 口径同款。
//   - 消除走 clear_stat_boosts 真原语（STAT_CLEAR 免消除挡下返回 0；不直写等级）。
//   - "每回合自身技能消除"按窗口被动结算：每回合 ROUND_START 无条件对对手施放一次
//     消除（被控/NONE 也结算）——与 759 逐回合 tick 同形态；官方文案未区分是否出手。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   1042 [回合]：25088 风夜笙歌 [3]、26526 无光之音 [3]、26603 金猪贺岁 [3]、
//     27502 邪恶意识 [3]、27806 心有神笔 [3]、28180 无念冰容 [4]、
//     37105 柔风咒 [3]、37164 混沌净化 [3]（3/4 两档）
//
// 动工前置三查（2026-10-22）：①内核 grep 1042 无硬编码 ②effect_info 无同文模板
//   ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(1042)——真缺口。
//
// 场景回归：test/scenario/scenario_5630_purge_boosts_window.cpp（号段 5630-5639）。

#include "common.h"

namespace {

// ROUND_START tick：payload = {owner, opp}，对对手消除一次正等级。
EffectResult j232_1042_round_purge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    if (opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    (void)g_core->clear_stat_boosts(ctx, opp);
    return EffectResult::kOk;
}

// ── 1042 —— {0}回合内每回合自身技能消除对手能力提升状态 ──
// int_args = [owner, target, 回合]（args_num=1）。
EffectResult effect_1042_purge_boosts_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1042, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1042, args,
                     "1042 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1042, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1042, args, "需要 回合 > 0");
    if (!g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int start_round = register_round_effect(
        ctx, State::BATTLE_ROUND_START, owner, 1042, rounds, {owner, opp},
        &j232_1042_round_purge);
    // 窗口含当回合（先手 / N==1）：ROUND_START 已过 → 立即补一次。
    if (start_round == ctx->roundCount) {
        (void)g_core->clear_stat_boosts(ctx, opp);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1042, effect_1042_purge_boosts_window)
