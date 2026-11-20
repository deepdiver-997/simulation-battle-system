// J 组 J178 · 窗粉伤族 —— 853×11 / 725×11（2026-10-04，worktree ../agents/j178）
//
// 家族语义（effect_info 官方模板即权威）：
//   853 [3] 附加自身最大体力值与速度值总和{0}%的百分比伤害，每次使用增加{1}%，最高{2}%
//   725 [3] {0}回合内每回合有{1}%的概率降低对手最大体力的1/{2}
//
// 实现口径：
//   - 853 = SKILL_EFFECT 直通（1366/869 递增直系）：pct = min({0}+{1}×(uses-1), {2})
//     （plugin_storage 每用 +1，795/2396 口径）→ deal_pink((max_hp+self SPEED 面板)×pct%,
//     PERCENT_VALUE)——双值和基数记档。
//   - 725 = InRounds({0}) ROUND_START tick（无条件每回合，字面）→ roll({1}%) →
//     deal_pink PERCENT_VALUE(对手 max_hp/{2})——"降低最大体力的1/{2}"按对手 max
//     折算粉伤字面。
//
// 载体切片（unity 库 json_each 精确匹配 11+11=22；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   853 [pct,step,cap]：{20,10,40}/{20,5,60}/… 无纯链——场景载体 王·毁灭星辰击
//     18331 [20,10,40] 物 pri0（链 180✓ purge 良性；760 未核 impl 时复核）
//   725  [回合,概率,分母]：{3,30,3}×2/{3,50,3}/… 纯链 万世荒芜决 17072 [3,30,3] 特 pri0
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 2 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：1366/869（审计槽递增，J175 前批）、796 形 deal_pink PERCENT_VALUE（1697）、
//   max_hp_of（common.h）、register_round_effect（InRounds 折算）。
// 场景回归：test/scenario/scenario_2800_pink_window2.cpp（2800 号段，一文件两宏）。

#include "common.h"

namespace {

constexpr int k853UsesBase = 990853000;   // +owner：连用计数（795/2396 口径）

// ── 853 —— 附加 (max_hp+速度)×{0}% 粉伤，连用递增封顶 ──
// int_args = [owner, target, pct, step, cap]（args_num=3）。
EffectResult effect_853_escalating_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 853, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 853, args,
                     "853 布局=[owner,target,pct,步长,上限]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     853, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0 && step > 0 && cap > 0, 853, args,
                     "需要 pct/步长/上限 > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int uses_key = k853UsesBase + owner;
    const auto it = ctx->plugin_storage.find(uses_key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[uses_key] = uses;
    const int pct_t = std::min(pct + step * (uses - 1), cap);
    const int base = max_hp_of(ctx, owner)
                   + ctx->ws.battle_attrs[owner][NumericalPropertyIndex::SPEED];
    const int pink = base * pct_t / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 725 tick：roll → 对手最大体力 1/{denom} 粉伤 ──
EffectResult j178_725_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->deal_pink_damage || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int chance = a.int_args[1];
    const int denom = a.int_args[2];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (!roll_percent(chance)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - self;
    const int dmg = max_hp_of(c, opp) / denom;
    if (dmg > 0) {
        (void)g_core->deal_pink_damage(c, opp, dmg, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

// ── 725 —— {0}回合内每回合有{1}%的概率降低对手最大体力的1/{2} ──
// int_args = [owner, target, rounds, chance, denom]（args_num=3）。
EffectResult effect_725_maxhp_roll_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 725, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 725, args,
                     "725 布局=[owner,target,回合,概率,分母]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     725, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && denom > 0, 725, args,
                     "需要 回合/概率/分母 > 0");
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 725, rounds,
                          {owner, chance, denom}, &j178_725_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(853, effect_853_escalating_pink)
SKILL_EFFECT(725, effect_725_maxhp_roll_window)
