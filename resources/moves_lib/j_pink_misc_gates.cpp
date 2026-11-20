// J 组 J168 · 粉体杂门族 —— 992×10 / 1461×10 / 1205×10（2026-10-04，
// worktree ../agents/j168）
//
// 家族语义（effect_info 模板即权威）：
//   992 （args_num=2）"{0}回合内攻击技能附加自身当前体力{1}%的百分比伤害"
//     —— 10 载体全为属技/零威力窗体（水何澹澹 [3,35]、覆海腾云翻 [3,30]…）。
//   1461（args_num=1）"恢复自身最大体力的1/{0}，体力低于对手时附加等量百分比伤害"
//     —— 判罚秘令 27864 [3]（纯链属）。
//   1205（args_num=1）"后出手时自身全属性+{0}"
//     —— 灵魂抽离 31310 [1]（纯链特）、沙暴重击 33792 [1]（纯链物）。
//
// 实现口径：
//   - 992 = 窗内骑乘：施放当回合即挂 EVENT_SKILL_USED watcher（duration=1，本回合）
//     + NextRounds({0}-1) ROUND_START tick 逐回合续挂——watcher lambda：ev.actor==owner
//     && !ev.used_attribute（攻击技门）→ deal_pink(对手, 当前体力×{1}%, PERCENT_VALUE)。
//     自身 EVENT_SKILL_USED 在 execute 起点发、早于本 SKILL_EFFECT → 施放当击不吃
//     （「攻击技能附加」=后续攻击语义，属技载体天然自洽）。
//   - 1461 = healed = heal_max_hp_fraction(自身, {0})（返回实际回复量）；
//     「体力低于对手」门在**回复前**求值（当前体力对比）→ deal_pink(对手, healed 等量)。
//   - 1205 = !acting_side_is_first → all_stats_up(自身, +{0})（六槽正增益不吃免弱，
//     common.h 直系）。
//
// 动工前置三查（2026-10-04）：①内核 grep 992/1461/1205 无硬编码 ②effect_meta 无分类
// 牵连 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2530_pink_misc_gates.cpp（号段 2530-2539）。

#include "common.h"

namespace {

// ── 992 的 ROUND_START tick：续挂当回合 EVENT_SKILL_USED 骑乘 watcher。──
void j168_arm_rider(BattleContext* ctx, int owner, int rounds_left, int pct);

EffectResult j168_rearm_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds_left = args.int_args[1];
    const int pct = args.int_args[2];
    const int effect_id = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds_left < 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    j168_arm_rider(ctx, owner, rounds_left, pct);
    (void)effect_id;
    return EffectResult::kOk;
}

void j168_arm_rider(BattleContext* ctx, int owner, int rounds_left, int pct) {
    // 本回合骑乘 watcher（duration=1：仅本回合内我的攻击技触发）
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/1,
        /*once=*/false,
        [owner, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 攻击技门（属性技不触发）
            }
            const int pink = c->getPet(owner).hp * pct / 100;
            if (pink > 0) {
                (void)g_core->deal_pink_damage(c, 1 - owner, pink, DamageKind::PERCENT_VALUE,
                                               owner);
            }
        },
        WatcherScope::TEAM);
    // 后续回合续挂（⚠️ rounds_left 必须递减——0 即封链，否则永久续期）
    if (rounds_left > 0) {
        (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 992,
                                          /*rounds=*/1,
                                          std::vector<int>{owner, rounds_left - 1, pct, 992},
                                          &j168_rearm_tick);
    }
}

// ── 992 —— {0}回合内攻击技能附加自身当前体力{1}%的百分比伤害 ──
// int_args = [owner, target, rounds, pct]（args_num=2）。
EffectResult effect_992_attack_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 992, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 992, args,
                     "992 布局=[owner,target,回合,pct]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 992, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 992, args, "需要 回合/pct > 0");
    j168_arm_rider(ctx, owner, rounds - 1, pct);
    return EffectResult::kOk;
}

// ── 1461 —— 恢复自身最大体力的1/{0}，体力低于对手时附加等量百分比伤害 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_1461_heal_equal_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1461, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1461, args,
                     "1461 布局=[owner,target,denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1461, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1461, args, "需要 denom > 0");
    if (!g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int hp_before = ctx->getPet(owner).hp;
    const int healed = heal_max_hp_fraction(ctx, owner, denom);   // 实际回复量
    if (hp_before < ctx->getPet(opp).hp && healed > 0) {
        // 「体力低于对手」（回复前求值）→ 等量百分比伤害
        (void)g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1205 —— 后出手时自身全属性+{0} ──
// int_args = [owner, target, delta]（args_num=1）。
EffectResult effect_1205_second_all_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1205, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1205, args,
                     "1205 布局=[owner,target,delta]");
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && delta > 0, 1205, args,
                     "需要 owner∈{0,1} delta>0");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门
    }
    all_stats_up(ctx, owner, delta);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(992, effect_992_attack_pink_window)
SKILL_EFFECT(1461, effect_1461_heal_equal_pink)
SKILL_EFFECT(1205, effect_1205_second_all_up)
