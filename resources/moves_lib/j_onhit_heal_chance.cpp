// J 组 J157 · 受击响应族四 —— 724×10（2026-10-04，worktree ../agents/j157）
//
// 家族语义（effect_info 模板即权威）：
//   724（args_num=3）"{0}回合内受到攻击{1}%恢复1/{2}体力"
//     —— 溯洄 28882 [4,50,1]（50% 回满）、烈炎滚滚 36664 [4,100,3]（100% 回 1/3）
//        （纯链 cat2 攻击技=武装即打，红伤自供）。
//
// 实现口径（J9 1322 受击 watcher 直系参数泛化）：
//   - EVENT_TAKE_DAMAGE watcher（InRounds {0} 窗、TEAM scope、once=false）：受击判据
//     = ev.target==self && damage_kind==NORMAL && amount>0（817/J9 口径：粉伤 tick/
//     真伤/属性直伤不构成「受到攻击」）→ roll_percent({1}) → heal_max_hp_fraction(1/{2})
//     （{2}=1 即回满；heal 吃封回血/恢复修正）。
//
// 动工前置三查（2026-10-04，工单行已核）：内核 grep 724 无硬编码 / battle_effects 无行 /
// moves_lib 并集无注册——真缺口。
// 场景回归：test/scenario/scenario_2480_onhit_heal_chance.cpp（号段 2480-2489）。

#include "common.h"

namespace {

// ── 724 —— {0}回合内受到攻击{1}%恢复1/{2}体力 ──
// int_args = [owner, target, rounds, chance, denom]（args_num=3）。
EffectResult effect_724_onhit_chance_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 724, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 724, args,
                     "724 布局=[owner,target,回合,概率,分母]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 724, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && denom > 0, 724, args,
                     "需要 回合/概率/分母 > 0");
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, denom](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.target != self || ev.damage_kind != static_cast<int>(DamageKind::NORMAL) ||
                ev.amount <= 0) {
                return;   // 受击判据（817/J9 口径）
            }
            if (!roll_percent(c->rng(), chance)) {
                return;
            }
            (void)heal_max_hp_fraction(c, self, denom);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(724, effect_724_onhit_chance_heal)
