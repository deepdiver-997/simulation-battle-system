// J 组 J177 · 申报兜底族三 —— 1007×11 / 1318×11（2026-10-04，worktree ../agents/j177）
//
// 家族语义（effect_info 官方模板即权威）：
//   1007 [4] {0}回合内每次攻击都有{1}%概率令对手{2}，未触发则对手全属性-{3}
//   1318 [2] {0}%的概率秒杀对手，未触发则令对手全属性-{1}
//
// 实现口径：
//   - 1007 = InRounds({0}) AFTER_ACTION 双探针（acting+executed_skill_is_attack 门，
//     924/1842 形；"每次攻击"按 1 次/回合近似，额外行动不计——J4 记档）：
//     roll({1}%) → apply_anomaly_checked；未落地（anomaly_landed 全档）→
//     all_stats_down(opp, {3})（J107 1003 窗形 + J13 兜底）。
//   - 1318 = J130 691 直系换兜底：495 AFTER_ACTION 延迟器（duration=1、acting 过滤、
//     HIT 门、存活门）→ roll({0}%) → force_hp_to_zero；未 roll 中 →
//     all_stats_down(opp, {1})。
//
// 载体切片（unity 库 json_each 精确匹配 11+11=22；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   1007 [回合,概率,码,降幅]：{3,70,6,1}/{3,50,6,1}/… 无纯链——场景载体 狂力弑天
//     25061 [3,70,6,1] cat4 pri0（链 191/843✓ 良性）
//   1318 [概率,降幅]：{6,1}/{5,1}×2/… 纯链×2（末世丧钟 32910 [5,1] 特140 pri0——
//     场景载体；码面无、降幅 1）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 2 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：J107 1003（申报+未触发 stat_drop）、J130 691（AFTER_ACTION 延迟器全套）、
//   924/1842（InRounds AFTER_ACTION 双探针）、J51 count_boost（all_stats_down 本地副本）。
// 场景回归：test/scenario/scenario_2790_prob_fallback5.cpp（2790 号段，一文件两宏）。

#include "common.h"

namespace {

bool j177_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

void j177_all_stats_down(BattleContext* ctx, int owner, int opp, int n) {
    if (!g_core || !g_core->stat_drop) {
        return;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, n, owner);
    }
}

// AFTER_ACTION 桶的当前行动方（J23/J109 同款小工具）。
int j177_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1007 tick：己方攻击后 roll 申报 / 未落地全属性降 ──
// payload = [owner, opp, chance, anomaly, drop, arm_round]。
EffectResult j177_1007_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[2];
    const int anomaly = a.int_args[3];
    const int drop = a.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (j177_bucket_acting_side(c) != self || !executed_skill_this_round(c, self)
        || !executed_skill_is_attack(c, self)) {
        return EffectResult::kOk;   // "每次攻击"门（1 次/回合近似，J4 记档）
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        c, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    if (!j177_anomaly_landed(r)) {
        j177_all_stats_down(c, self, opp, drop);   // 未触发兜底
    }
    return EffectResult::kOk;
}

// ── 1007 —— {0}回合内每次攻击都有{1}%概率令对手{2}，未触发则对手全属性-{3} ──
// int_args = [owner, target, rounds, chance, anomaly, drop]（args_num=4）。
EffectResult effect_1007_attack_prob_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1007, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1007, args,
                     "1007 布局=[owner,target,回合,概率,异常码,降幅]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int drop = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1007, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && drop > 0, 1007, args,
                     "需要 回合/概率/降幅 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1007, args,
                     "需要 异常码 ∈ [0,43]");
    std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    payload.push_back(ctx->roundCount);   // arm_round（NextRounds 口径见下）
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1007, rounds, payload, &j177_1007_tick);
    }
    return EffectResult::kOk;
}

// ── 1318 probe：命中且存活 → roll 秒杀 / 未中 → 全属性降 ──
EffectResult j177_1318_kill_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->force_hp_to_zero || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[2];
    const int drop = a.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (j177_bucket_acting_side(c) != self) {
        return EffectResult::kOk;
    }
    if (c->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 命中门（J130 口径）
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死
    }
    if (!roll_skill_percent(*c, 1318, 0, self, opp, chance)) {
        j177_all_stats_down(c, self, opp, drop);   // 未触发兜底
        return EffectResult::kOk;
    }
    (void)g_core->force_hp_to_zero(c, opp, self);
    return EffectResult::kOk;
}

// ── 1318 —— {0}%的概率秒杀对手，未触发则令对手全属性-{1} ──
// int_args = [owner, target, chance, drop]（args_num=2）。
EffectResult effect_1318_prob_kill_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1318, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1318, args,
                     "1318 布局=[owner,target,概率,降幅]");
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int drop = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1318, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && drop > 0, 1318, args, "需要 概率/降幅 > 0");
    const std::vector<int> payload{owner, 1 - owner, chance, drop};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1318, 0, owner, -1, EffectArgs(payload), &j177_1318_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1318;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1007, effect_1007_attack_prob_window)
SKILL_EFFECT(1318, effect_1318_prob_kill_fallback)
