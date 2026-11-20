// J 组 J305 · 增伤+对手回血族 —— 1883×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1883（args_num=2）"造成的攻击伤害提升{0}%，若未击败对手则对手回合结束后
//   其恢复最大体力的1/{1}"
//
// 实现口径：
//   - 增伤半 = SKILL_EFFECT 时点 `ws.damage_add_pct[owner] += {0}`（通用加法增伤通道；
//     693/776/1486 同款；ws 每回合 reset → 只作用本次技能）。
//   - 回血半 = FIRST/SECOND AFTER_ACTION 双桶探针（669 同形 acting_side 判据）：
//     本侧行动结束且对手仍存活 → register_round_effect(BATTLE_ROUND_END, rounds=1)
//     → 回合末 heal_amount(对手, 对手最大体力/{1})（heal 原语，吃封回血/恢复修正）。
//     "若未击败对手" = 死亡则不注册（死宠不回血）。
//   - 行参在位即消费：{0} 显式 0 = 不增伤；{1} 显式 0/负 = 不回血（不回退默认分母）。
//   - 审计槽：991883100+self = 本次增伤 pct；991883200+self = 回合末回血量。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽）：
//   **m38003 渊影之欣快 [1883]|[30,4]（纯链，cat2，场景载体）** /
//   m36228 紧急拘捕令 → [100,3] / m36584 浣得浣失 → [50,3] / m37187 寒结冰境 → [75,2] /
//   m37848 悖论回旋镖 → [50,2] / m38140 鲎爪回旋镖 → [50,3] / m38198 喧哗禁止 → [50,3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1883 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1883)——真缺口。
//
// 场景回归：test/scenario/scenario_6380_amp_foe_heal.cpp（号段 6380-6389）。

#include "common.h"

namespace {

constexpr int kAmpAuditBase = 991883100;    // +self：本次增伤 pct
constexpr int kHealAuditBase = 991883200;   // +self：回合末回血量

void j305_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

int j305_max_hp_of(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// AFTER_ACTION 桶的当前行动方（j_gate_family4 j38 同款小工具）。
int j305_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1883 回合末回血 tick —— payload=[self, opp, denom] ──
EffectResult j305_1883_round_end_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已倒下 → 不回血
    }
    const int amount = j305_max_hp_of(ctx->getPet(opp)) / denom;
    if (amount > 0 && g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, opp, amount);
    }
    j305_set_storage(ctx, kHealAuditBase + self, amount);
    return EffectResult::kOk;
}

// ── 1883 行动末探针：未击败 → 注册本回合末对手回血 ──
EffectResult j305_1883_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1
        || j305_bucket_acting_side(ctx) != self) {
        return EffectResult::kOk;   // 只在本侧行动结束那格结算
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 击败门：倒下则不回血
    }
    (void)register_round_effect(ctx, State::BATTLE_ROUND_END, self, 1883,
                                /*rounds=*/1, {self, opp, denom},
                                &j305_1883_round_end_heal);
    return EffectResult::kOk;
}

}  // namespace

// ── 1883 —— 攻击伤害提升{0}%；未击败则对手回合末回 maxHP/{1} ──
// int_args = [owner, target, pct, denom]（args_num=2）。
EffectResult effect_1883_amp_foe_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1883, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1883, args,
                     "1883 布局=[owner,target,pct,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1883,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：{0}/{1} 显式 0 合法（不增伤 / 不回血），负值才是布局错误。
    EFFECT_ARG_GUARD(pct >= 0 && denom >= 0, 1883, args, "需要 pct/分母 ≥ 0");
    // 增伤半：本次技能通用加法增伤（ws 每回合 reset）。
    if (pct > 0) {
        ctx->ws.damage_add_pct[self] += pct;
    }
    j305_set_storage(ctx, kAmpAuditBase + self, pct);
    j305_set_storage(ctx, kHealAuditBase + self, 0);
    // 回血半：行动末探针（击败门在行动末判）。
    const std::vector<int> payload{self, opp, denom};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1883, 0, self, -1, EffectArgs(payload), &j305_1883_after_action);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1883;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1883, effect_1883_amp_foe_heal)
