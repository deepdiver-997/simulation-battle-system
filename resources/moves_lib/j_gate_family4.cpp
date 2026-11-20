// J38 · 未击败/击败门族四 —— 1107×19 / 678×20 / 669×20（2026-10-02，worktree ../agents/j38）
//
// 家族语义（effect_info 官方模板 + 4399 寰宇圣芒决"当回合未胜则下3回合攻击+1"核对），
// 三条都是"当回合击败/未击败 → 门后缀"：
//
// 1107（arg_count=4）"未击败对手则{0}%自身全属性+{1}，{2}%对手全属性-{3}"：
//   - J12 897 AFTER_ACTION 桥共门（对手在场宠 hp>0，duration=1 行动方过滤）；
//   - 双段独立 roll：roll_percent({0}) → all_stats_up(自身, +{1})；roll_percent({2}) →
//     stat_drop 六槽(对手, {3})。全库 19 载体 {0}/{2} 全为 50、{1}/{3} 全为 1
//     （场景用 12 局独立对照统计带）。
// 678（arg_count=3）"当回合未击败对手则下{0}回合每回合{1}+{2}"：
//   - 同门；{1}=能力槽下标（全载 0=攻击）、{2}=每回合增量（全载 1）——4399
//     寰宇圣芒决"下3回合攻击+1"逐位互证；NextRounds({0}) ROUND_START tick
//     stat_change(自身, {1}, +{2})（827 载荷形，ws 重置无碍——levels 持久可叠）。
// 669（arg_count=0）"当回合击败对手则下回合自身攻击先制+1"：
//   - **击败门**（709 同构反向：AFTER_ACTION 时对手在场宠 hp<=0）；后缀 =
//     NextRounds(1) 的 MOVE_RIGHT tick 写 ws.preemptive_level[自身] += 1
//     （843/499 通道；下回合选择期生效，先手权比较前）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1107/678/669 无行为硬编码 ②effect_meta
// 无分类牵连 ③moves_lib（本分支）无既有注册——真缺口。载体数核对（EXISTS）：
// 19/20/20；全量切片核验 59 载体布局零异常（1107 [50,1,50,1] 均一 / 678
// [rounds,槽,增量] / 669 空参）。
// 先例：J12 897/J37 957（AFTER_ACTION 桥正反门）、827/681（NextRounds tick）、
//   843/499（MOVE_RIGHT 先制）、709（击败门）、all_stats_up（common.h）。
// 场景回归：test/scenario/scenario_1230_gate_family4.cpp（1230 号段，一文件三宏）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（897/J12 同款小工具，插件各自持有一份）。
int j38_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1107 共用门探针：未击败 → 双段概率升降 ──
EffectResult j38_1107_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_drop || !a.int_args || a.int_count < 6) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int pct_up = a.int_args[2];
    const int up = a.int_args[3];
    const int pct_down = a.int_args[4];
    const int down = a.int_args[5];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (j38_bucket_acting_side(c) != self || c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判；击败了不触发
    }
    if (roll_percent(pct_up)) {
        all_stats_up(c, self, up);
    }
    if (roll_percent(pct_down)) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            (void)g_core->stat_drop(c, opp, i, down, self);
        }
    }
    return EffectResult::kOk;
}

// ── 1107 —— 未击败对手则{0}%自身全属性+{1}，{2}%对手全属性-{3} ──
EffectResult effect_1107_gate_dual_prob(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1107, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1107, args,
                     "布局=[owner,target,pct1,up,pct2,down]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1107, args, "owner 越界");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[4] > 0
                         && args.int_args[5] > 0, 1107, args, "需要 四参数 > 0");
    const std::vector<int> payload(args.int_args, args.int_args + 6);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1107, 0, self, -1, EffectArgs(payload), &j38_1107_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1107;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 678 tick：每回合 {1}槽 +{2} ──
EffectResult j38_678_boost_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_change || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int slot = a.int_args[2];
    const int per = a.int_args[3];
    if (self < 0 || self > 1 || slot < 0 || slot >= BattleContext::kAbilityLevelSlotCount
        || per <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(c, self, slot, per);
    return EffectResult::kOk;
}

// ── 678 —— 当回合未击败对手则下{0}回合每回合{1}+{2} ──
// ── 678 共用门探针：未击败 → 武装"下{0}回合每回合{1}+{2}"窗 ──
EffectResult j38_678_gate_arm(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_change || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1 || j38_bucket_acting_side(c) != self) {
        return EffectResult::kOk;
    }
    if (c->getPet(1 - self).hp <= 0) {
        return EffectResult::kOk;   // 未击败门
    }
    register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 678, a.int_args[2],
                                {self, 1 - self, a.int_args[3], a.int_args[4]},
                                &j38_678_boost_tick);
    return EffectResult::kOk;
}

EffectResult effect_678_gate_roundly_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 678, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 678, args,
                     "布局=[owner,target,rounds,slot,per]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 678, args, "owner 越界");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[4] > 0, 678, args,
                     "需要 rounds>0 && per>0");
    EFFECT_ARG_GUARD(args.int_args[3] >= 0
                         && args.int_args[3] < BattleContext::kAbilityLevelSlotCount,
                     678, args, "槽位越界");
    const std::vector<int> payload(args.int_args, args.int_args + 5);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(678, 0, self, -1, EffectArgs(payload), &j38_678_gate_arm);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 678;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 669 tick：MOVE_RIGHT 写攻击先制 +1 ──
EffectResult j38_669_preempt_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    c->ws.preemptive_level[self] += 1;
    return EffectResult::kOk;
}

// ── 669 共用门探针：击败对手 → 武装"下回合攻击先制+1" ──
EffectResult j38_669_gate_arm(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1 || j38_bucket_acting_side(c) != self) {
        return EffectResult::kOk;
    }
    if (c->getPet(1 - self).hp > 0) {
        return EffectResult::kOk;   // 击败门（709 反向同判）
    }
    register_next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, self, 669,
                                /*rounds=*/1, {self, 1 - self},
                                &j38_669_preempt_tick);
    return EffectResult::kOk;
}

// ── 669 —— 当回合击败对手则下回合自身攻击先制+1（零参）──
EffectResult effect_669_kill_preempt_next(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 669, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 669, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 669, args, "owner 越界");
    const std::vector<int> payload{self, 1 - self};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(669, 0, self, -1, EffectArgs(payload), &j38_669_gate_arm);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 669;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1107, effect_1107_gate_dual_prob)
SKILL_EFFECT(678, effect_678_gate_roundly_boost)
SKILL_EFFECT(669, effect_669_kill_preempt_next)
