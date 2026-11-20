// J 组 J203 · 未击败随机全PP归零族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   739 [1] 当回合未击败对手则有{0}%将对手所有技能PP值降为0
//
// 实现口径：
//   - SKILL_EFFECT 注册本回合 ROUND_END 的 1 回合窗（register_round_effect；
//     N=1 即本回合，先/后出手都算）——"当回合"= 本次施放所在回合。
//   - tick：对手在场宠 hp>0（"未击败"）→ 概率 {0}% 掷点（roll_percent）→
//     逐槽 pp_zero_slot（f_misc_family 716/2299 同原语；直写 pp 会绕过事件）。
//   - 审计槽 plugin_storage[k739AuditBase+self] = rolled*1000 + 实际清零槽数
//     （场景无随机精确断言：rolled=0 时末三位=0，rolled=1 时=实际槽数）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   739 [概率]：36805 撕心之刎 纯链 [30]（物 140，场景载体）、17166 神话·星河破灭 [50]；
//        其余链式载体按各自 arg_count 顺序切分后 739 段均为概率值（9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 739 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(739)——真缺口。
// 场景回归：test/scenario/scenario_5280_unbeaten_pp_zero.cpp（号段 5280-5289）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k739AuditBase = 990739100;   // +self：rolled*1000 + 清零槽数

// ── 739 ROUND_END tick：未击败 → 概率清空对手全技能 PP ──
EffectResult effect_739_unbeaten_pp_zero_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || chance <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 当回合已击败 → 不触发
    }
    const bool rolled = roll_percent(chance);
    int landed = 0;
    if (rolled && g_core && g_core->pp_zero_slot) {
        const int slots = static_cast<int>(ctx->getPet(opp).skills.size());
        for (int i = 0; i < slots; ++i) {
            if (g_core->pp_zero_slot(ctx, opp, i, self) == PpReduceResult::SUCCESS) {
                ++landed;
            }
        }
    }
    ctx->plugin_storage[k739AuditBase + self] = (rolled ? 1000 : 0) + landed;
    return EffectResult::kOk;
}

// ── 739 ──
EffectResult effect_739_unbeaten_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 739, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 739, args,
                     "739 布局=[owner,target,概率]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     739, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && chance <= 100, 739, args, "需要 概率∈(0,100]");
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 739, /*rounds=*/1,
                          {self, opp, chance}, &effect_739_unbeaten_pp_zero_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(739, effect_739_unbeaten_pp_zero)
