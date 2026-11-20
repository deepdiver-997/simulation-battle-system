// J 组 J251 · 防御同调族 —— 45×9（2026-10-24，worktree ../agents/j251，orchestrator 代开）
//
// 家族语义（effect_info 官方模板即权威）：
//   45 [1] {0}回合自身的防御力和对手相同
//
// 实现口径（51 变化术直系，j_power_view3.cpp 同构）：
//   SKILL_EFFECT 立即拷 battle_attrs[自身][DEFENSE]=battle_attrs[对手][DEFENSE]
//   （武装回合即生效）+ InRounds({0}) ROUND_START tick 逐回合续拷（每回合
//   sync_workspace_from_on_stage 重灌 battle_attrs → 拷贝自愈，窗口过期自然还原）。
//   ⚠️ 只拷防御字面（row 口径，51 的"只拷物攻"同款）：特防槽不动——特攻打过来
//   读的是特防，不受本效果影响（字面口径，非"防御+特防全同调"）。
//
// 载体切片（unity 库 json 解析精确匹配 pure=3 mixed=6）：
//   45 [回合]：纯链 模仿术 20029 [5] / 真实残影 20336 [3] / 幻像残影 20788 [5]（全 cat4）；
//   混链 6 行（45 与其他效果同链，场景不用）。
//
// 动工前置三查（2026-10-24 本 worktree 复核）：
//   ①内核 grep "防御力和对手相同"/45 无硬编码 case；②effect_meta 无行；
//   ③moves_lib 并集无 SKILL_EFFECT(45 注册——真缺口。
//   （注：effect_icon 的 tips 行与 45 无关——45 无 tips 行，官方模板即权威。）
// 先例：51（{0}回合自身的攻击力和对手相同，j_power_view3：battle_attrs 拷 +
//   register_round_effect ROUND_START tick 续拷）。
// 场景回归：test/scenario/scenario_7020_defense_sync.cpp（7020 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 45 tick：防御力续拷（每回合 sync 重灌 → 拷贝自愈）──
EffectResult j251_45_copy_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    c->ws.battle_attrs[self][NumericalPropertyIndex::DEFENSE] =
        c->ws.battle_attrs[1 - self][NumericalPropertyIndex::DEFENSE];
    return EffectResult::kOk;
}

// ── 45 —— {0}回合自身的防御力和对手相同 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_45_copy_defense(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 45, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 45, args,
                     "45 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     45, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 45, args, "需要 回合 > 0");
    // 武装回合立即生效一次（本轮对手打过来即同防）；后续回合由 ROUND_START tick 续拷。
    ctx->ws.battle_attrs[self][NumericalPropertyIndex::DEFENSE] =
        ctx->ws.battle_attrs[opp][NumericalPropertyIndex::DEFENSE];
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 45, rounds, {self},
                          &j251_45_copy_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(45, effect_45_copy_defense)
