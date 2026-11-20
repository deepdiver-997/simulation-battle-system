// J43 · 护盾爆发/嘲讽惩罚族 —— 1017×20 / 767×20（2026-10-02，worktree ../agents/j43）
//
// 家族语义（effect_info 官方模板），两条都是"资源/状态条件的固伤"：
//
// 1017（arg_count=0）"消耗自身所有护盾值，造成等量固定伤害"：
//   - shield_bank_.total() 读账 → consume(total, &broken)（**公开方法直呼**——
//     battle_primitives 的 consume_shield 自由函数内核内不暴露插件，但其逻辑就
//     是"方法调用 + 逐条 emit EVENT_SHIELD_BROKEN"，插件侧手动补发同事件即可——
//     K2 2263/混地 2318 主动消耗同语义："视为被击破"喂魂印破盾子句）；
//   - 等量固伤 = fixed_damage(对手, consumed)（1662 镇魂约凡撒"护盾值炸固伤"同款，
//     盘点白话卡 ❌ 项之一）；无盾（consumed=0）→ 不打 0 伤。
//   - 实例：无情岩爆斩 31284、冲天战意斩 31062（全库 20 载体）。
// 767（arg_count=2）"{0}回合内每回合使用技能且出手流程结束后若对手处于能力下降
//     状态则附加给对手{1}点固定伤害"：
//   - InRounds {0} 回合的 AFTER_ACTION 双态 tick（FIRST/SECOND 各一条——
//     register_round_effect 两次调用，2099 同款；bucket_acting_side 过滤行动方）；
//   - tick 判 used_skill_this_round（"使用技能"）&& 对手任一负槽 <0（"处于能力
//     下降状态"，count_boost 反向）→ fixed_damage(对手, {1})。每回合独立结算。
//   - 实例：木灵低吟 26743 [3,300]、幽静哀声 24902 [3,200]（全库 20 载体）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1017/767 无行为硬编码（consume_shield
//   自由函数是 K2 主动消耗原语，本族经公开方法等价到达）②effect_meta 无分类牵连
//   ③moves_lib（本分支）无既有注册——真缺口。载体数核对（EXISTS）：20/20；全量
//   切片核验 40 载体布局零异常（1017 空参 / 767 [rounds,amount]）。
// 先例：K2 2263/2318（consume_shield 语义与事件）、1662（盾值转固伤，白话卡）、
//   2099（AFTER_ACTION 双态窗）、897（行动方过滤）、808（fixed_damage）。
// 场景回归：test/scenario/scenario_1280_shield_punish.cpp（1280 号段，一文件两宏）。

#include "common.h"

#include <vector>

namespace {

// AFTER_ACTION 桶的当前行动方（897 同款小工具，插件各自持有一份）。
int j43_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1017 —— 消耗自身所有护盾值，造成等量固定伤害（零参）──
EffectResult effect_1017_shield_burst(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1017, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1017, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 1017, args,
                     "owner/target 越界");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int total = ctx->getPet(self).shield_bank_.total();
    if (total <= 0) {
        return EffectResult::kOk;   // 无盾：消耗 0 → 不打 0 伤
    }
    int broken = 0;
    const int consumed = ctx->getPet(self).shield_bank_.consume(total, &broken);
    for (int i = 0; i < broken; ++i) {
        ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, -1, self});
    }
    if (consumed > 0) {
        (void)g_core->fixed_damage(ctx, opp, consumed);
    }
    return EffectResult::kOk;
}

// ── 767 tick：出手流程结束 + 对手有下降 → 附加 {1} 固伤 ──
EffectResult j43_767_punish_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->fixed_damage || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int amount = a.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (j43_bucket_acting_side(c) != self || !used_skill_this_round(c, self)) {
        return EffectResult::kOk;   // "使用技能且出手流程结束后"
    }
    bool foe_has_drop = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (c->ability_levels[opp][i] < 0) {
            foe_has_drop = true;
        }
    }
    if (!foe_has_drop) {
        return EffectResult::kOk;   // 前提：对手处于能力下降状态
    }
    (void)g_core->fixed_damage(c, opp, amount);
    return EffectResult::kOk;
}

// ── 767 —— {0}回合内每回合使用技能且出手流程结束后若对手处于能力下降状态则附加
//    给对手{1}点固定伤害 ──
EffectResult effect_767_drop_punish_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 767, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 767, args,
                     "布局=[owner,target,rounds,amount]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 767, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 767, args, "需要 rounds>0 && amount>0");
    // InRounds AFTER_ACTION 双态窗（2099 同款两次注册；行动方过滤在 tick 内）。
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, self, 767, rounds,
                          {self, 1 - self, rounds, amount}, &j43_767_punish_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, self, 767, rounds,
                          {self, 1 - self, rounds, amount}, &j43_767_punish_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1017, effect_1017_shield_burst)
SKILL_EFFECT(767, effect_767_drop_punish_window)
