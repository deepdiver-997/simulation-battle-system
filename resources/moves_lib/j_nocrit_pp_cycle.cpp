// J 组 J300 · 未暴击PP循环族 —— 1689×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1689（args_num=2）"本回合未打出致命一击则恢复自己所有技能{0}点PP值
//   并降低对手所有技能{1}点PP值"
//
// 实现口径：
//   - 判据 = 本回合**未打出致命一击**：`ctx->crit_happened[self]`（skills.cpp 每次
//     技能使用先清、伤害时点置位；AFTER_ACTION 读到的是本次技能的结果）。
//   - 时机 = BATTLE_FIRST/SECOND_AFTER_ACTION 双桶探针（j_gate_family4 669 同形）：
//     施放时各挂一条 duration=1 的探针，只在"本侧行动结束"那格执行
//     （acting_side 判据 897/J12 同款）。
//   - 落点 = pp_restore(self, {0}, only_empty=false) + pp_reduce(opp, {1})（原语；
//     无限 PP 槽自动跳过；pp_reduce 逐槽 clamp ≥0）。
//   - 行参在位即消费：{0}/{1} 显式 0 = 不执行该半（原语本身对 amount<=0 返回 0，
//     这里仍如实调用/审计，不设默认值）。
//   - 审计槽：991689100+self = 本次暴击旗（1=暴击→不补）；991689200+self =
//     pp_restore 变更槽数；991689300+self = pp_reduce 结果（0=SUCCESS）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，6/7 自洽）：
//   **m35071 火中生莲华 [1530,1483,1689]|[200,2,1,1] → 1689=[1,1]（场景载体；
//   1530 遇天敌真伤/1483 暴击封属性均良性）** / m35894 龙舞·仪 → [1,1] /
//   m36517 泯辉极明破 → [1,1] / m36893 圣霆之调律 → [2,2] / m37030 白金彻鸣闪 →
//   [1,1] / m37362 曲止杜鹃啼 → [1,1] /
//   ⚠️ m38374 寞路羔羊的追悔 切片欠参（链上效果合计 5 参、DB 只给 5 但 1689 段为空）
//   ——数据工单候选，不选作载体。
//
// 动工前置三查（2026-10-22）：①内核 grep 1689 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1689)——真缺口。
//
// 场景回归：test/scenario/scenario_6330_nocrit_pp_cycle.cpp（号段 6330-6339）。

#include "common.h"

namespace {

constexpr int kCritAuditBase = 991689100;     // +self：暴击旗
constexpr int kRestoreAuditBase = 991689200;  // +self：恢复变更槽数
constexpr int kReduceAuditBase = 991689300;   // +self：pp_reduce 结果

// AFTER_ACTION 桶的当前行动方（j_gate_family4 j38 同款小工具，插件各自持有一份）。
int j300_bucket_acting_side(const BattleContext* ctx) {
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

void j300_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 1689 行动末探针：未暴击 → 自回 PP + 削对手 PP ──
// payload = [self, opp, restore, reduce]。
EffectResult j300_1689_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int restore = args.int_args[2];
    const int reduce = args.int_args[3];
    if (j300_bucket_acting_side(ctx) != self) {
        return EffectResult::kOk;   // 只在本侧行动结束那格结算
    }
    const bool crit = ctx->crit_happened[self];
    j300_set_storage(ctx, kCritAuditBase + self, crit ? 1 : 0);
    if (crit) {
        return EffectResult::kOk;   // 本回合打出致命一击 → 整条不执行
    }
    int changed = 0;
    if (g_core && g_core->pp_restore) {
        changed = g_core->pp_restore(ctx, self, restore, /*only_empty=*/false);
    }
    j300_set_storage(ctx, kRestoreAuditBase + self, changed);
    if (g_core && g_core->pp_reduce) {
        const PpReduceResult r = g_core->pp_reduce(ctx, opp, reduce);
        j300_set_storage(ctx, kReduceAuditBase + self, static_cast<int>(r));
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 1689 —— 本回合未打出致命一击则自回 {0} PP、削对手 {1} PP ──
// int_args = [owner, target, restore, reduce]（args_num=2）。
EffectResult effect_1689_nocrit_pp_cycle(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1689, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1689, args,
                     "1689 布局=[owner,target,自回,削敌]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int restore = args.int_args[2];
    const int reduce = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1689,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0 合法（该半不生效），负值才是布局错误。
    EFFECT_ARG_GUARD(restore >= 0 && reduce >= 0, 1689, args, "需要 自回/削敌 ≥ 0");
    const std::vector<int> payload{self, opp, restore, reduce};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1689, 0, self, -1, EffectArgs(payload), &j300_1689_after_action);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1689;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1689, effect_1689_nocrit_pp_cycle)
