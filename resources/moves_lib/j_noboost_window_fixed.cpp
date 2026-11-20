// J 组 J179 · 无提升窗固伤族 —— 1027×11（2026-10-04，worktree ../agents/j179）
//
// 家族语义（effect_info 官方模板即权威）：
//   1027（arg_count=2）"{0}回合内若自身不处于能力提升状态则附加{1}点固定伤害"
//
// 实现口径：
//   - InRounds({0}) AFTER_ACTION 双探针（1462 直系换门）：acting+executed_skill
//     门 → 无正槽（count_boost 六槽扫反向，J51 判据）→ fixed_damage({1})。
//     每回合附加一次（"回合内附加"字面；攻击门=属性技载体武装不算——字面"若…
//     则附加"无攻击限定词，与 1462 的"使用技能附加"区分记档：本条按回合附加、
//     不设攻击门——载体全 cat4 属性技佐证）。
//
// 载体切片（unity 库 json_each 精确匹配 11/11；顺序切分 side_effect.arg_count 全载体
// 自洽、零剩余，脚本核算 2026-10-04）：1027 [回合,固伤]：{4,350}/{3,300}/{3,200}/…
//   全载体 cat4 属性技（纯武装；狂乱双颅 25061 [4,350] pri0——链 585/48✓ 良性，
//   场景载体）。
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 1027
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：1462（InRounds AFTER_ACTION 窗附伤形）、count_boost（J51）、
//   fixed_damage（CoreApi）。
// 场景回归：test/scenario/scenario_2810_noboost_window_fixed.cpp（2810 号段）。

#include "common.h"

namespace {

// ── 1027 tick：无正槽 → 附加 {fixed} 固伤 ──
EffectResult j179_1027_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->fixed_damage || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int fixed = a.int_args[3];   // payload=全量 args [owner,target,rounds,fixed]
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || fixed <= 0) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (c->ability_levels[self][i] > 0) {
            return EffectResult::kOk;   // "不处于能力提升状态"门
        }
    }
    (void)g_core->fixed_damage(c, opp, fixed);
    return EffectResult::kOk;
}

// ── 1027 —— {0}回合内若自身不处于能力提升状态则附加{1}点固定伤害 ──
// int_args = [owner, target, rounds, fixed]（args_num=2）。
EffectResult effect_1027_noboost_window_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1027, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1027, args,
                     "1027 布局=[owner,target,回合,固伤]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1027, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && fixed > 0, 1027, args, "需要 回合/固伤 > 0");
    const std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1027, rounds, payload, &j179_1027_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1027, effect_1027_noboost_window_fixed)
