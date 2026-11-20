// J 组 J297 · 未受击门封属性族 —— 1031×8（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1031（args_num=1）"出手时本回合若未受到伤害则对手下{0}回合属性技能无效"
//
// 实现口径（同形先例直系）：
//   - 门 = **本回合未受到任意伤害**：j_untouched_gate.cpp 1708 的受击旗直系——
//     施放时挂全战场长 EVENT_TAKE_DAMAGE watcher（TEAM），挨打置
//     plugin_storage[991031100 + roundCount*2 + side]；结算读当回合格。
//   - 结算锚 = BATTLE_OLD_ROUND_END_1（整回合口径，1708 同款；arm_round 守卫防同回合
//     早执行）。门开 → seal_skill(属性回合型)：attribute=true / attack=false /
//     count=0 / duration_rounds={0}+1（锚点补 1，见下）/ penetrable=true（919 同款）/ ON_STAGE +
//     source_slot=on_stage[owner]（施放方换宠清）。
//   - 行参在位即消费：{0} 显式 0/负 = 不封（不设默认窗长）。
//   - 回合型封属的授予窗补 1：OLD_ROUND_END_1 之后紧跟本回合 tick_rounds，
//     不补会把"下 1 回合"当场扣光（实测记档）。
//   - ⚠️ 首施放盲区（1708 已记档）：watcher 自首次施放才置旗——**首次施放回合施放前**
//     已发生的受击不可见。本效果模板"出手时"通常发生在对手行动前，影响有限；
//     场景 T6300.b 用**第二回合**的受击把门闭死，绕开盲区。
//   - 审计槽：991031200+owner = 本次门结果（1=封/0=未封）；
//     991031201+owner = 本次读到的受击旗（0/1）。
//
// 载体切片（side_effect.arg_count 优先、args_num 兜底，2026-10-22 核算，8/8 自洽）：
//   m19925 幽茫千世决 [1031]|[1]（**纯链，cat1 攻击技，场景载体**）/ m30601 [1031]|[1] /
//   m36442 [1031]|[1] / m27776 → [2] / m30130 → [1] / m35298 → [1] /
//   m35933 → [3] / m37277 → [3]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1031 无行为硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1031)——真缺口。
//
// 场景回归：test/scenario/scenario_6300_untouched_seal_attr.cpp（号段 6300-6309）。

#include "common.h"

namespace {

constexpr int kFlagAnyBase = 991031100;    // +roundCount*2+side：本回合受击旗
constexpr int kGateAuditBase = 991031200;  // +owner：门结果
constexpr int kFlagAuditBase = 991031201;  // +owner：本次读到的旗

bool j297_hit_flag(const BattleContext* ctx, int side) {
    const auto it = ctx->plugin_storage.find(
        static_cast<unsigned long long>(kFlagAnyBase + ctx->roundCount * 2 + side));
    return it != ctx->plugin_storage.end() && std::any_cast<int>(it->second) != 0;
}

void j297_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// 全战场长受击旗 watcher（1708 同款）：本侧成为 EV TAKE_DAMAGE 的 target 即置旗。
void j297_arm_flag_watcher(BattleContext* ctx, int self) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, /*start=*/0, /*rounds=*/9999, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;
            }
            c->plugin_storage[static_cast<unsigned long long>(
                kFlagAnyBase + c->roundCount * 2 + self)] = 1;
        },
        WatcherScope::TEAM);
}

// ── 1031 settle —— OLD_ROUND_END_1：门开则封对手属性技能 {0} 回合 ──
// payload = [self, opp, rounds, arm_round]。
EffectResult j297_1031_settle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int arm_round = args.int_args[3];
    if (arm_round != ctx->roundCount) {
        return EffectResult::kOk;   // 同回合早执行守卫（1708 同款）
    }
    const bool hit = j297_hit_flag(ctx, self);
    j297_set_storage(ctx, kFlagAuditBase + self, hit ? 1 : 0);
    if (hit) {
        j297_set_storage(ctx, kGateAuditBase + self, 0);
        return EffectResult::kOk;   // 本回合已受击 → 门闭
    }
    if (!g_core || !g_core->seal_skill || rounds <= 0) {
        j297_set_storage(ctx, kGateAuditBase + self, 0);
        return EffectResult::kOk;
    }
    // 本 settle 锚在 OLD_ROUND_END_1，**本回合的 tick_rounds 紧随其后**会立刻扣掉
    // 一回合 → 授予时补 1，实际生效窗恰为下 {rounds} 回合 [R+1, R+rounds]。
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/1031,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds + 1, /*penetrable=*/true,
                             /*source_slot=*/ctx->on_stage[self], EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    j297_set_storage(ctx, kGateAuditBase + self, 1);
    return EffectResult::kOk;
}

}  // namespace

// ── 1031 —— 出手时本回合若未受到伤害则对手下{0}回合属性技能无效 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_1031_untouched_seal_attr(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1031, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1031, args,
                     "1031 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1031,
                     args, "需要 owner/target ∈ {0,1}");
    j297_arm_flag_watcher(ctx, self);
    const std::vector<int> payload{self, opp, rounds, ctx->roundCount};
    (void)register_round_effect(ctx, State::BATTLE_OLD_ROUND_END_1, self, 1031,
                                /*rounds=*/1, payload, &j297_1031_settle);
    return EffectResult::kOk;
}

SKILL_EFFECT(1031, effect_1031_untouched_seal_attr)
