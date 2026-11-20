// J 组 J169 · 反转封回血族 —— 1675×12（2026-10-04，worktree ../agents/j169）
//
// 家族语义（effect_info 官方模板即权威）：
//   1675（arg_count=1）"反转对手能力提升状态，反转成功则对手{0}回合内无法通过自身
//   技能恢复体力"
//
// 实现口径：
//   - stat_boost_reversal(opp) == REVERSED 门（978/J111 反转族同判据）→
//     grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), 0, {0}, ...)
//     （1449/797 封回血票直系——e_drain_heal_family.cpp:172 调用形；heal_impl 消费）。
//     无提升反转失败（NOTHING/BLOCKED）→ 无后缀。
//
// 载体切片（unity 库 json_each 精确匹配 12/12；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-04）：1675 [回合]：{2}×4/{3}×4/{1}×2/{5}×2。
//   无纯链——场景载体 霜谕天寒叹 35198 [2] pri0（链 1522✓/1029✓ 已落地，atk=1 低伤
//   不触发其高阈值面）。
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 1675
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：stat_boost_reversal（CoreApi 99）、grant_immunity+HEAL_BLOCK（CoreApi 形，
//   e_drain_heal_family 1449/effect_batch8 797）、coverage_all（state.h 65）。
// 场景回归：test/scenario/scenario_2710_reversal_healblock.cpp（2710 号段）。

#include "common.h"

namespace {

// ── 1675 —— 反转对手提升；成功则对手{0}回合内无法通过自身技能恢复体力 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_1675_reversal_healblock(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1675, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1675, args,
                     "1675 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1675, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1675, args, "需要 回合 > 0");
    if (!g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, opp) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // "反转成功"门（无可反/被挡下）
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/1675, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1675);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1675, effect_1675_reversal_healblock)
