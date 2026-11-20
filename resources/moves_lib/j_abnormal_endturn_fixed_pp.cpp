// J 组 J199 · 异常/降态回合末固伤+削PP窗族 —— 1 宏/10 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   1731 [3] {0}回合内每回合结束后若双方任意一方处于异常状态或能力下降状态
//            则令对手受到{1}点固定伤害且所有技能PP值-{2}
//
// 实现口径：
//   - 窗 = register_round_effect(BATTLE_ROUND_END, {0})（common.h helper：走
//     round_effect_start_round 的"N回合内"口径；持续窗由 ContinuousEffect 自动到期，
//     不手写 plugin_storage 账本——套路目录 §三）。
//   - tick 条件：**双方任意一方** has_any_active_abnormal_status(side)
//     或 六槽任一负能力等级（能力下降；能力等级换宠清空，符合"处于"语义）。
//   - 落点：fixed_damage(opp,{1})（复用 deal_damage，吃护盾/事件）+ pp_reduce(opp,{2})
//     （对手所有技能 PP -N；PP 操作走原语，直写会绕过 EVENT_PP_REDUCED）。
//   - 审计槽 plugin_storage[k1731AuditBase+self] = 本窗累计触发回合数（场景可断言
//     "哪几回合真的触发"，无随机）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1731 [回合,固伤,削PP]：36845 拈红渲紫 纯链 [4,300,2]（物 140，场景载体）、
//        28301 栗栗辛苦 [300,2]、27750 灵鱼双生 [300,1]、28341 枫下祈愿 [300,1]…
//        （10/10 自洽；{0} 2~8，{1} 200~300，{2} 1~2）
//
// 动工前置三查（2026-10-22）：①内核 grep 1731 无硬编码（17313/17323 为 LIKE 噪声）
// ②custom_effect_overrides/programs 无行 ③moves_lib 并集（含 kbatch worktree）无
// SKILL_EFFECT(1731)——真缺口。
// 场景回归：test/scenario/scenario_5220_abnormal_endturn_fixed_pp.cpp（号段 5220-5229）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1731AuditBase = 990173100;   // +self：本窗累计触发回合数

bool j199_any_side_in_condition(const BattleContext* ctx, int self, int opp) {
    if (ctx->has_any_active_abnormal_status(self)
        || ctx->has_any_active_abnormal_status(opp)) {
        return true;   // 任意一方处于异常状态
    }
    for (int side = 0; side <= 1; ++side) {
        for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
            if (ctx->ability_levels[side][slot] < 0) {
                return true;   // 任意一方处于能力下降状态
            }
        }
    }
    return false;
}

// ── 1731 tick：回合结束后判定 + 固伤 + 削 PP ──
// int_args = [owner, target, 固伤, 削PP点数]（args_num=3）。
EffectResult effect_1731_endturn_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    const int pp_points = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp
        || amount <= 0 || pp_points <= 0) {
        return EffectResult::kOk;
    }
    if (!j199_any_side_in_condition(ctx, self, opp)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, amount);
    }
    if (g_core && g_core->pp_reduce) {
        (void)g_core->pp_reduce(ctx, opp, pp_points);
    }
    const auto it = ctx->plugin_storage.find(k1731AuditBase + self);
    const int ticks = (it == ctx->plugin_storage.end()) ? 0 : any_int_or(it->second);
    ctx->plugin_storage[k1731AuditBase + self] = ticks + 1;
    return EffectResult::kOk;
}

// ── 1731 ──
EffectResult effect_1731_abnormal_endturn_fixed_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1731, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1731, args,
                     "1731 布局=[owner,target,回合,固伤,削PP]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    const int pp_points = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1731, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0 && pp_points > 0, 1731, args,
                     "需要 回合/固伤/削PP > 0");
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 1731, rounds,
                          {self, opp, amount, pp_points}, &effect_1731_endturn_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1731, effect_1731_abnormal_endturn_fixed_pp)
