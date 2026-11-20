// J 组 J301 · 击败先制族 —— 1587×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1587（args_num=1）"当回合击败对手则下回合自身攻击先制+{0}"
//
// 实现口径（同形先例直系）：
//   - 击败门 = j_gate_family4.cpp **669**（"当回合击败对手则下回合自身攻击先制+1"）
//     同形：施放时在 FIRST/SECOND AFTER_ACTION 各挂一条 duration=1 探针
//     （acting_side 判据 897/J12 同款）；本侧行动结束且 `getPet(opp).hp <= 0` →
//     register_next_rounds_effect(BATTLE_FIRST_MOVE_RIGHT, rounds=1)。
//   - MOVE_RIGHT tick = 481/J4 `j4_attack_priority_tick` 判据：
//     used_skill_this_round(owner) && selected_skill_is_attack(owner)（选择期语义——
//     MOVE_RIGHT 时技能尚未执行）→ ws.preemptive_level[owner] += {0}。
//     ⚠️ 669 的 tick 未做攻击限定（属性技也会加分）；本条按模板"**攻击**先制"与
//     481/J4 的既定判据收紧，场景 T6340.c 显式覆盖属性技不加分。
//   - 行参在位即消费：{0} 显式 0/负 = 不移交 bonus（仍武装探针，审计记 0）。
//   - 审计槽：991587100+self = 击败门（1/0）；991587200+self = 实际写入的先制值。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽）：
//   **m35724 斩魂音刃 [1587]|[1]（纯链，cat1，场景载体）** / m34247 天下归心斩 → [2] /
//   m34523 无念无忧散 → [1] / m34756 鼠鼠威风击 → [2] / m34862 月·惊梦曲 → [2] /
//   m37173 默日悲音 → [2] / m37237 祸肇·干戈嚣 → [3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1587 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1587)——真缺口
//   （j_boost_cond_anomaly 注释已记"1587 击败先制——未接静默"）。
//
// 场景回归：test/scenario/scenario_6340_kill_preempt.cpp（号段 6340-6349）。

#include "common.h"

namespace {

constexpr int kGateAuditBase = 991587100;     // +self：击败门
constexpr int kBonusAuditBase = 991587200;    // +self：实际写入先制

// AFTER_ACTION 桶的当前行动方（j_gate_family4 j38 同款小工具）。
int j301_bucket_acting_side(const BattleContext* ctx) {
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

void j301_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 1587 MOVE_RIGHT tick：攻击技限定 → ws.preemptive_level += bonus ──
EffectResult j301_preempt_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[1];
    if (self < 0 || self > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    // MOVE_RIGHT 桶：技能尚未执行 → 选择位判据（481/J4 同款）。
    if (!used_skill_this_round(ctx, self) || !selected_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;   // "攻击先制"限定：属性技不加分
    }
    ctx->ws.preemptive_level[self] += bonus;
    j301_set_storage(ctx, kBonusAuditBase + self, bonus);
    return EffectResult::kOk;
}

// ── 1587 击败门探针：本侧行动结束且对手已倒 → 武装下回合攻击先制 ──
EffectResult j301_kill_gate(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[1];
    if (self < 0 || self > 1 || j301_bucket_acting_side(ctx) != self) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(1 - self).hp > 0) {
        j301_set_storage(ctx, kGateAuditBase + self, 0);
        return EffectResult::kOk;   // 击败门未开
    }
    j301_set_storage(ctx, kGateAuditBase + self, 1);
    if (bonus <= 0) {
        return EffectResult::kOk;   // 行参显式 0 = 不移交（探针照常武装）
    }
    (void)register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 1587,
                                      /*rounds=*/1, {self, bonus}, &j301_preempt_tick);
    return EffectResult::kOk;
}

}  // namespace

// ── 1587 —— 当回合击败对手则下回合自身攻击先制+{0} ──
// int_args = [owner, target, bonus]（args_num=1）。
EffectResult effect_1587_kill_preempt(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1587, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1587, args,
                     "1587 布局=[owner,target,先制]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int bonus = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1587,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0 合法（不移交），负值才是布局错误。
    EFFECT_ARG_GUARD(bonus >= 0, 1587, args, "需要 先制 ≥ 0");
    const std::vector<int> payload{self, bonus};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1587, 0, self, -1, EffectArgs(payload), &j301_kill_gate);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1587;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1587, effect_1587_kill_preempt)
