// J18 · 暴击条件族 —— 1204×23 / 1483×21（2026-10-02，worktree ../agents/j18）
//
// 家族语义（effect_info 官方模板），两条都以"致命一击"为轴：
//
// 1204（arg_count=1）"自身下{0}次攻击必定致命一击"：
//   - NextRounds({0}) 的 ROUND_START tick 写 ws.must_crit（681 必暴窗同通道——工单
//     模板先例即 681；58 注释：crit_rate=0 的技能乘算修正表达不了"必定"必须走
//     must_crit 短路）。"次≈回合"口径记档见实现处（一回合至多一次出手；跳回合烧窗，
//     {0} 全库实载 {1,2}；严格次数窗的计数方案在 EffectFn 无捕获限制下不取）。
//   - 实例：灼烈之风 28331 [2]、强化骨骼 27959 [1]（全库 23 载体，{0}∈{1,2}）。
// 1483（arg_count=1）"本回合打出致命一击则令对手{0}回合属性技能无效"：
//   - 判定 = AFTER_ACTION 桥（J12/897 同构双探针、duration=1、行动方过滤）读
//     crit_happened[自身]——query_usage 掷好、每次技能使用先清（skills.cpp:743），
//     到自身行动结束时点的真值即"本回合(我的行动)打出过致命一击"；
//   - 后果 = seal_skill 属性封（478 逐参同款：attribute=true、attack=false、
//     hit_invalid=false、penetrable=true、ON_STAGE）——封属窗口按票面记账
//     （register_round 起算 remaining_rounds：R1 打出暴击 → R1、R2 对手属性技无效）。
//   - 实例：幽魂铁蹄 33760 [1]（全库 21 载体，{0}∈{1,2}，[2]×18）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1204/1483 无行为硬编码（crit_happened 是
//   通用旗，非专设）②effect_meta 无分类牵连 ③moves_lib 无既有注册——真缺口。
//   载体数核对：23/21 与工单一致；全量切片脚本核验 44 载体布局零异常。
// 先例：58/681（ws.must_crit ROUND_START tick）、897（shared_ptr 计数）、
//   EVENT_SKILL_USED（E13，469 同款注册）、J12/897（AFTER_ACTION 桥）、478（封属）。
// 场景回归：test/scenario/scenario_1030_crit_cond.cpp（1030 号段，一文件两宏）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（897/J12 同款小工具，插件各自持有一份）。
int j18_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1204 共用：ROUND_START tick —— counts>0 则写必暴（58/681 同通道）──
EffectResult j18_1204_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[self] = true;
    return EffectResult::kOk;
}

// ── 1204 —— 自身下{0}次攻击必定致命一击 ──
// ⚠️ 口径记档（2026-10-02）：模板是"次"不是"回合"，但一回合至多一次出手（暴击每
// 次技能使用只掷一回）→ NextRounds({0}) 的 ROUND_START tick 写 ws.must_crit
// （681 必暴窗同通道——工单模板先例即 681；58 注释：crit_rate=0 的技能乘算修正
// 表达不了"必定"）。近似面 = 跳过回合会烧窗（被控/嗑药回合不补）；{0} 全库实载
// {1,2}。曾试"shared_ptr 计数 + SKILL_USED 消费"的严格次数窗：EffectFn 无捕获
// 限制下 tick 拿不到计数（同键覆盖风险 > 收益），不取。
EffectResult effect_1204_next_attacks_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1204, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1204, args,
                     "布局=[owner,target,times]");
    const int self = args.int_args[0];
    const int times = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1204, args, "owner 越界");
    EFFECT_ARG_GUARD(times > 0, 1204, args, "需要 times>0");
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1204, times,
                                {self, 1 - self}, &j18_1204_must_crit_tick);
    return EffectResult::kOk;
}

// ── 1483 后缀探针（无捕获 EffectFn）：自身行动结束时点判本回合暴击 → 封属 ──
EffectResult j18_1483_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->seal_skill || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int rounds = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (j18_bucket_acting_side(c) != self || !c->crit_happened[self]) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判；本回合没打出暴击不封
    }
    // 478 逐参同款封属（SEAL_ATTRIBUTE、可穿、非命中失效）。
    (void)g_core->seal_skill(c, /*source=*/self, /*target=*/opp, /*effect_id=*/1483,
                             /*attribute=*/true, /*attack=*/false,
                             /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1483 —— 本回合打出致命一击则令对手{0}回合属性技能无效 ──
EffectResult effect_1483_crit_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1483, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1483, args,
                     "布局=[owner,target,rounds]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1483, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 1483, args, "需要 rounds>0");
    // AFTER_ACTION 桥（J12 同构）：crit 在 query_usage 掷定、行动结束后仍可读；
    // 探针里不再往本桶注册效果（seal 走 RuleCenter）→ 无同键覆盖顾虑。
    const std::vector<int> payload{self, 1 - self, rounds};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1483, 0, self, -1, EffectArgs(payload), &j18_1483_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1483;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1204, effect_1204_next_attacks_must_crit)
SKILL_EFFECT(1483, effect_1483_crit_seal_attribute)
