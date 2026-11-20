// J144 · 条件固伤族 —— 515×10 / 685×10 / 1027×11（2026-10-04，worktree ../agents/j144）
//
// 家族语义（effect_info 官方模板 + 载体切片核对），三条都是"自身状态条件 → 附加固伤"：
//
// 515（args_num=1）"自身处于能力下降状态时附加{0}点固定伤害，并解除这些能力下降状态"：
//   - 门 = 任一负槽（六槽含命中，J 组 has_any_stat_drop 同判据）→ fixed_damage(对手,{0})
//     + clear_stat_drops(自身)（core 原语清全部负槽，"解除这些能力下降状态"）；
// 685（args_num=1）"若自身处于能力提升状态，附加{0}点固定伤害"：
//   - 门 = 任一正槽（has_any_positive_boost 同判据）→ fixed_damage(对手,{0})；
// 1027（args_num=2）"{0}回合内若自身不处于能力提升状态则附加{1}点固定伤害"：
//   - InRounds({0}) 窗（597 同款 AFTER_ACTION 双桶 tick，施放回合即含）每回合判
//     !has_any_positive_boost(自身) → fixed_damage(对手,{1})；模板无"使用技能"字样
//     → 不设 used_skill 门（NONE 行动也判，记档）。
//   - 固伤不吃 final、无伤害阈值 → 全部 SKILL_EFFECT 即时/窗挂，不走 DETECT（工单口径）。
//
// 动工前置三查（2026-10-04，立单时已核）：①内核 grep 3 id 无行为硬编码
//   ②battle_effects/custom_effect_overrides 无行 ③moves_lib 全 worktree 并集无注册。
//   切片：[固伤]/[固伤]/[回合,固伤] 31/31 自洽（515 {0}∈{100,150,300,400}；
//   685 {0}∈{90,100,120,150,200,250,300}；1027 {0}∈{3,4},{1}∈{300,350,400}）。
// 先例：J14 has_any_stat_drop/J82 has_any_positive_boost（六槽判据）、J2 1170（固伤
//   原语）、J15 597（round_window AFTER_ACTION 双桶）、J141（clear 系原语互证）。
// 场景回归：test/scenario/scenario_2370_cond_fixed.cpp（2370 号段，一文件三宏）。

#include "common.h"

namespace {

// 任一正槽（六槽含命中；e_priority/j_cond_output2 同判据，就地重写）。
bool j144_has_any_boost(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 任一负槽（六槽含命中；j_first_second_gates 同判据，就地重写）。
bool j144_has_any_drop(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// 1027 的窗 tick：payload = [self, opp, fixed]。
EffectResult j144_1027_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (j144_has_any_boost(ctx, self)) {
        return EffectResult::kOk;   // 处于能力提升状态：不附加
    }
    (void)g_core->fixed_damage(ctx, opp, fixed);
    return EffectResult::kOk;
}

}  // namespace

// ── 515 —— 自身处于能力下降状态时附加{0}点固定伤害，并解除这些能力下降状态 ──
// int_args = [owner, target, fixed]（args_num=1）。
EffectResult effect_515_drop_cond_fixed_cure(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 515, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 515, args,
                     "515 布局=[owner,target,固伤]");
    const int self = args.int_args[0];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     515, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 515, args, "需要 固伤 > 0");
    if (!j144_has_any_drop(ctx, self)) {
        return EffectResult::kOk;   // 无下降状态：门不开
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, args.int_args[1], fixed);
    }
    if (g_core && g_core->clear_stat_drops) {
        (void)g_core->clear_stat_drops(ctx, self);   // "并解除这些能力下降状态"
    }
    return EffectResult::kOk;
}

// ── 685 —— 若自身处于能力提升状态，附加{0}点固定伤害 ──
// int_args = [owner, target, fixed]（args_num=1）。
EffectResult effect_685_boost_cond_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 685, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 685, args,
                     "685 布局=[owner,target,固伤]");
    const int self = args.int_args[0];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     685, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 685, args, "需要 固伤 > 0");
    if (!j144_has_any_boost(ctx, self)) {
        return EffectResult::kOk;   // 无提升状态：门不开
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, args.int_args[1], fixed);
    }
    return EffectResult::kOk;
}

// ── 1027 —— {0}回合内若自身不处于能力提升状态则附加{1}点固定伤害 ──
// int_args = [owner, target, rounds, fixed]（args_num=2）。
EffectResult effect_1027_noboost_round_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1027, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1027, args,
                     "1027 布局=[owner,target,回合,固伤]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1027, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && fixed > 0, 1027, args, "需要 回合/固伤 > 0");
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1027, rounds, {self, 1 - self, fixed},
                              &j144_1027_round_tick);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(515, effect_515_drop_cond_fixed_cure)
SKILL_EFFECT(685, effect_685_boost_cond_fixed)
SKILL_EFFECT(1027, effect_1027_noboost_round_fixed)
