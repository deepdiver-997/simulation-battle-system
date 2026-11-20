// J92 · 百分比窗族 —— 1213×13（2026-10-03，worktree ../agents/j92）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1213（args_num=2）"{0}回合内每回合使用技能附加对手最大体力1/{1}的百分比伤害"：
//   - 窗 = NextRounds({0}) AFTER_ACTION 双侧探针（J70 同形；起点 R+1——载体当次
//     不附加，"每回合"从下回合起算）；
//   - tick = 自身 acting（桶 per-owner 保证）+ 真用技（used_skill_this_round，J6 口径）
//     → deal_pink_damage(PERCENT_VALUE, 对手最大体力/{1})（E10 703 同款换算源：
//     amount 传折算后的绝对值）。
//   - ⚠️ 同模板 1693 的"对手免疫百分比伤害时额外真伤"子句需粉免检测面（pink 管线
//     免伤旗），本单不含——1693 另立记档。
//   - 实例：皇权在握 25597 [1001,57,1213,1011]|[1,4,3,4,3,3,60]（属性技；⚠️ 参差
//     载体：57 args_num=2 → 1213 实切 [4,3]，尾 [3,60] 未绑定，记档；1001/57/1011
//     全自侧效果）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1213 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1213=2 与 effect_info.args_num 一致；13 载体切片自洽（[回合,分母]）。
// 先例：J70（NextRounds AFTER_ACTION 窗 + used_skill 门）、E10 703（PERCENT_VALUE
//   换算源）、J61 878（武装回合守卫——防同回合早执行）。
// 场景回归：test/scenario/scenario_1810_percent_window.cpp（1810 号段）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（J12 同款小工具）。
int j92_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// 窗 tick：自身每回合用技 → 对手掉最大体力 1/{denom} 百分比伤。
// payload = [owner, opp, rounds, denom, arm_round]（arm_round 防同回合早执行，J61 同款）。
EffectResult j92_percent_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->deal_pink_damage || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int denom = a.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (c->roundCount <= a.int_args[4]) {
        return EffectResult::kOk;   // 武装当回合不附加
    }
    if (j92_bucket_acting_side(c) != self || !used_skill_this_round(c, self)) {
        return EffectResult::kOk;   // 每回合用技才附加（嗑药/空转不算）
    }
    const int amount = c->getPet(opp).hp > 0 ? max_hp_of(c, opp) / denom : 0;
    if (amount > 0) {
        (void)g_core->deal_pink_damage(c, opp, amount, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 1213 —— {0}回合内每回合使用技能附加对手最大体力1/{1}的百分比伤害 ──
// int_args = [owner, target, rounds, denom]（args_num=2）。
EffectResult effect_1213_percent_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1213, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1213, args,
                     "1213 布局=[owner,target,rounds,denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1213, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 1213, args, "需要 回合/分母 > 0");
    std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    payload.push_back(ctx->roundCount);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(ctx, st, owner, 1213, rounds, payload,
                                    &j92_percent_tick);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1213, effect_1213_percent_window)
