// J109 · 双防粉/吸取窗族 —— 944×13 / 924×13（2026-10-03，worktree ../agents/j109）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 944（args_num=2）"{0}回合内对手使用攻击技能时受到自身防御、特防总和{1}%的百分比
//   伤害"：
//   - J30 676 变体（触发面翻转）：EVENT_SKILL_USED watcher 挂 **对手** 名下（J97
//     勘误：触发面=对手行动，注册到对手侧才会执行）+ ev.actor==opp + !used_attribute
//     （攻击技）门 → deal_pink_damage(opp, 双防视图和×{1}%)（PERCENT_VALUE）；
//   - 窗 = InRounds({0}) 含当回合（先手武装挡当回合后手那一下为期望语义），
//     start/expire 自卡（676 同款）。
//   - 实例：泰坦结界 [48,47,944]|[4,4,4,40]（48/47 自侧免票已注册良性；944=[4,40]）。
// 924（args_num=2）"下{0}回合攻击技能吸取对手1/{1}最大体力值"：
//   - 窗 = NextRounds({0}) AFTER_ACTION 双侧探针（J61/J70 同形 + arm_round 守卫——
//     "下{0}回合"从 R+1 起）；tick 三门：自身 acting + used_skill + 攻击技 →
//     drain_hp(自身, 对手, {1})（CoreApi 163，无相谛通道）。
//   - 实例：智能形变 35849 [924]|[2,3]（特120 纯链：2 回合·吸 1/3）。
//
// 动工前置三查（2026-10-03）：①内核 grep 944/924 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   944=2/924=2 与 effect_info.args_num 一致；26 载体切片自洽。
// 先例：J30 676（SKILL_USED watcher/面板读法/自卡窗）、J97（对手侧注册勘误）、
//   J61/J70（NextRounds + arm_round 守卫）、drain_hp（CoreApi 163）。
// 场景回归：test/scenario/scenario_2030_foeattack_defpink_drain.cpp（2030 号段）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（J12 同款小工具）。
int j109_bucket_acting_side(const BattleContext* ctx) {
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

// 924 tick：自身每回合用攻击技 → 吸对手 1/{denom} 最大体力。
// payload = [owner, opp, rounds, denom, arm_round]。
EffectResult j109_drain_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->drain_hp || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int denom = a.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (c->roundCount <= a.int_args[4]) {
        return EffectResult::kOk;   // 武装当回合不吸（"下{0}回合"从 R+1 起）
    }
    if (j109_bucket_acting_side(c) != self || !executed_skill_this_round(c, self)
        || !executed_skill_is_attack(c, self)) {
        return EffectResult::kOk;   // "攻击技能"才吸
    }
    (void)g_core->drain_hp(c, self, opp, denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 944 —— {0}回合内对手使用攻击技能时受到自身双防总和{1}%的百分比伤害 ──
// int_args = [owner, target, rounds, pct]（args_num=2）。
EffectResult effect_944_foeattack_def_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 944, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 944, args,
                     "944 布局=[owner,target,rounds,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     944, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 944, args, "需要 回合/pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    const int duration = (start - ctx->roundCount) + rounds;
    // ⚠️ 挂对手名下（触发面=对手行动，J97 勘误）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, opp, ctx->roundCount, duration, /*once=*/false,
        [owner, opp, pct, start, expire](BattleContext* c, const BattleEvent& ev) {
            if (!c || c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗自卡（InRounds 含当回合）
            }
            if (ev.actor != opp || ev.used_attribute || ev.target < 0 || ev.target > 1) {
                return;   // 条件：对手使用攻击技能
            }
            const int def_sum =
                c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE)
                + c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE);
            const int pink = def_sum * pct / 100;
            if (pink > 0) {
                (void)g_core->deal_pink_damage(c, opp, pink, DamageKind::PERCENT_VALUE,
                                               owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 924 —— 下{0}回合攻击技能吸取对手1/{1}最大体力值 ──
// int_args = [owner, target, rounds, denom]（args_num=2）。
EffectResult effect_924_attack_drain_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 924, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 924, args,
                     "924 布局=[owner,target,rounds,denom]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     924, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 924, args, "需要 回合/分母 > 0");
    std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    payload.push_back(ctx->roundCount);   // arm_round 守卫（J61/J70 同款）
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(ctx, st, owner, 924, rounds, payload,
                                    &j109_drain_tick);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(944, effect_944_foeattack_def_pink)
SKILL_EFFECT(924, effect_924_attack_drain_window)
