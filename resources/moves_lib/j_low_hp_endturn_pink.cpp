// J 组 J202 · 低体回合末比例粉伤族 —— 1 宏/11 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   888 [1] 自己出手时若自身体力低于对手则回合结束时减少对手1/{0}最大体力
//
// 实现口径：
//   - SKILL_EFFECT（自己出手时）**快照**条件 self.hp < opp.hp（严格）；成立才注册
//     本回合 ROUND_END 的 1 回合窗（register_round_effect；N=1 即本回合，先/后手都算）。
//   - 回合末落点 = deal_pink_damage(opp, max_hp(opp)/{0}, PERCENT_VALUE, self)
//     （1136/1137 体力比例粉伤同通道：走粉伤相 IMMUNE/REDUCE，可被护罩吸收）。
//   - 条件只在出手时判定一次（快照），回合末体力再变化不影响本次结算（字面口径）。
//   - 审计槽 plugin_storage[k888AuditBase+self] = 本次粉伤值（场景精确断言用）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   888 [分母]：30052 绝地打击 纯链 [4]（物 130，场景载体）、
//        36815 [4]、37436 [4] 等（11 载体，分母 3~4 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 888 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(888)——真缺口。
// 场景回归：test/scenario/scenario_5270_low_hp_endturn_pink.cpp（号段 5270-5279）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k888AuditBase = 990888100;   // +self：本次粉伤值

// ── 888 回合末 tick：减少对手 1/{0} 最大体力（粉伤·百分比档）──
EffectResult effect_888_endturn_pink_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || denom <= 0) {
        return EffectResult::kOk;
    }
    const int pink = max_hp_of(ctx, opp) / denom;
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k888AuditBase + self] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

// ── 888 ──
EffectResult effect_888_low_hp_endturn_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 888, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 888, args,
                     "888 布局=[owner,target,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     888, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 888, args, "需要 分母 > 0");
    if (ctx->getPet(self).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // 出手时快照：自身体力严格低于对手才成立
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 888, /*rounds=*/1,
                          {self, opp, denom}, &effect_888_endturn_pink_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(888, effect_888_low_hp_endturn_pink)
