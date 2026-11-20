// J 组 J211 · 本次伤害比例回合粉伤窗族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   829 [2] 下{0}回合对手每回合受到相当于本次伤害的1/{1}的百分比伤害（可以叠加）
//
// 实现口径：
//   - DETECT 攻方桶读本次攻击 final（首刀 armed/round 双闸），amount = final/{1}；
//     随后注册 **NextRounds {0} 回合**的 ROUND_END tick（register_next_rounds_effect
//     的构造方式：round_effect_start_round(..., NextRounds) → 下回合起算）。
//   - **叠加**：每条施放注册一个独立 ContinuousEffect（source_id_=0 → TimedBucket
//     自增 key，不去重），N 次施放 = N 条各自到期，回合末逐条结算自己的 amount。
//   - tick = deal_pink_damage(opp, amount, PERCENT_VALUE)（粉伤相；1136/1137 同通道）。
//   - 审计槽 plugin_storage[k829AuditBase+self] = 最近一次捕获的每回合粉伤值。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   829 [回合,分母]：33033 嗜血蝙蝠 纯链 [2,3]（特 130，场景载体）、
//        32565 超空间突袭 [2,3]、35231 片叶断山海 [3,3]、37551 渗宇洄光 [3,3]…
//        （9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 829 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(829)——真缺口。
// 场景回归：test/scenario/scenario_5370_captured_damage_pink_window.cpp（号段 5370-5379）。

#include "common.h"

#include <primitives/battle_primitives.h>

#include <memory>

namespace {

constexpr int k829AuditBase = 990829100;   // +self：最近捕获的每回合粉伤值

// ── 829 tick：每回合受到捕获伤害的 1/{1}（粉伤·百分比档）──
EffectResult effect_829_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || amount <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

// ── 829 —— 下{0}回合对手每回合受到本次伤害的1/{1}（可叠加）──
// int_args = [owner, target, 回合, 分母]（args_num=2）。
EffectResult effect_829_captured_damage_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 829, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 829, args,
                     "829 布局=[owner,target,回合,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     829, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 829, args, "需要 回合/分母 > 0");
    auto armed = std::make_shared<bool>(true);
    auto registered_round = std::make_shared<int>(ctx->roundCount);
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, opp, rounds, denom, armed, registered_round](BattleContext* c,
                                                            int bucket_owner) {
            if (!c || bucket_owner != self || !*armed) {
                return;
            }
            if (c->roundCount != *registered_round) {
                return;   // 首刀未结算 → 不留跨回合误捕
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId != opp || !d.isRed) {
                return;
            }
            *armed = false;
            const int amount = d.final / denom;
            if (amount <= 0) {
                return;
            }
            c->plugin_storage[k829AuditBase + self] = amount;
            // 下 N 回合（NextRounds 起点），每条施放独立条目 → 叠加。
            Effect e(829, 0, self, rounds, EffectArgs(std::vector<int>{self, opp, amount}),
                     &effect_829_tick);
            const int start = c->round_effect_start_round(
                self, rounds, EffectWindowKind::NextRounds);
            auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, self,
                                                         rounds, start);
            ce->source_id_ = 0;   // 0 = TimedBucket 自增 key，不去重（叠加语义）
            c->register_skill_effect(State::BATTLE_ROUND_END, self, std::move(ce));
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(829, effect_829_captured_damage_pink_window)
