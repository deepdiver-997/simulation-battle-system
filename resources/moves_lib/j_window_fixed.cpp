// J 组 J171 · 窗附伤族 —— 1462×13（2026-10-04，worktree ../agents/j171）
//
// 家族语义（effect_info 模板即权威）：
//   1462（args_num=2）"{0}回合内每回合使用技能附加{1}点固定伤害，附加时若自身不处于
//   能力提升状态则固定伤害翻倍"
//     —— 落英缤纷 26681 [3,200]（属，链 191/843 已实现良性）等 13 链。
//
// 实现口径（工单行裁定：924/1842 双探针形 + executed 门）：
//   - InRounds({0}) 窗，AFTER_ACTION **双探针**（FIRST/SECOND_AFTER_ACTION 各注册一条，
//     owner 自卡——先/后手两个时间线都覆盖，每回合恰一条触发）。
//   - tick 门 = executed_skill_this_round(自身)（2026-10-03 判据统一：skill_use_seq
//     增量口径，被控/嗑药不算使用）→ fixed_damage(对手, {1})；
//     无正槽门（六槽无正等级，J51 count_boost 同判据）→ ×2（「附加时」=施加时点求值）。
//   - InRounds 含当回合：施放本身即「使用技能」→ 施放当回合 AFTER_ACTION 首跳 {1}×m。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04，13/13
// [回合,固伤] 自洽零剩余）。
// 动工前置三查（2026-10-04，工单行已核含 CARRY）：真缺口。
// 场景回归：test/scenario/scenario_2730_window_fixed.cpp（号段 2730-2739）。

#include <cstdio>

#include "common.h"

namespace {

// 无正槽（六槽含命中，J51 count_boost 同判据）。
bool j171_has_no_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return false;
        }
    }
    return true;
}

// 一跳：使用技能门 + 无提升翻倍 → fixed_damage。
void j171_apply_once(BattleContext* ctx, int owner, int target, int fixed) {
    if (!g_core || !g_core->fixed_damage) {
        return;
    }
    if (!executed_skill_this_round(ctx, owner)) {
        return;   // 「每回合使用技能」门（skill_use_seq 增量判据，被控/嗑药不算）
    }
    const int amount = j171_has_no_boost(ctx, owner) ? fixed * 2 : fixed;
    (void)g_core->fixed_damage(ctx, target, amount);
}

// ── 1462 的 ROUND_END tick —— payload={owner, target, fixed, cast_round, rounds}。
// ⚠️ 实测（2026-10-04 探针）：roundCount 在回合完成时才递增——施放当回合的 ROUND_END
//   桶里 roundCount 仍是施放回合值，NextRounds(start=R+1) 的「下回合才生效」对 tick
//   不成立（J70 arm 守卫的 deadline 若按施放回合折算会漏挡早执行）。守卫改相对区间：
//   tick 只在 roundCount ∈ (cast_round, cast_round+rounds-1] 时生效。
EffectResult j171_window_fixed_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int fixed = args.int_args[2];
    const int cast_round = args.int_args[3];
    const int rounds = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount <= cast_round || ctx->roundCount > cast_round + rounds - 1) {
        return EffectResult::kOk;   // 早执行/过窗守卫（首跳已落）
    }
    j171_apply_once(ctx, owner, target, fixed);
    return EffectResult::kOk;
}

// ── 1462 —— {0}回合内每回合使用技能附加{1}点固定伤害（无提升翻倍）──
// 首跳切分（J14 1024 范式）：施放当回合即首跳（施放本身=使用技能）；2..{0} 走
// NextRounds ROUND_END tick（J155 已证过期正确；AFTER_ACTION 探针窗实测不清退，弃用
// ——探针记档）。int_args = [owner, target, rounds, fixed]（args_num=2）。
EffectResult effect_1462_window_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1462, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1462, args,
                     "1462 布局=[owner,target,回合,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1462, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && fixed > 0, 1462, args, "需要 回合/固伤 > 0");
    // 首跳（施放当回合，使用技能门天然成立）
    j171_apply_once(ctx, owner, target, fixed);
    // 后续 2..{0} 跳（相对区间守卫见 tick）
    if (rounds > 1) {
        (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, owner, 1462,
                                          rounds - 1,
                                          std::vector<int>{owner, target, fixed,
                                                           ctx->roundCount, rounds},
                                          &j171_window_fixed_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1462, effect_1462_window_fixed)
