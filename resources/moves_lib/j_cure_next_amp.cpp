// J141 · 解除异常增伤族 —— 631×12（2026-10-04，worktree ../agents/j141）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 631（args_num=2）"消除{0}状态，消除成功下回合造成伤害提升{1}%"：
//   - 解除门 = cure_anomalies(自身, {0})（core 原语，治愈数返回 0=失败——
//     2158 解除自身异常族同通道）；载体为攻击技，自身带{0}状态时施放即自愈；
//   - "下回合造成伤害提升{1}%" = NextRounds(1) ROUND_START tick 写
//     ws.damage_add_pct（J55 652/1204 必暴窗同款窗骨，damage_add_pct 为
//     776 通用增伤加算通道、ws 每回合 reset 天然单回合有效）。
//
// 动工前置三查（2026-10-04，立单时已核）：①内核 grep 631 无行为硬编码
//   ②battle_effects/custom_effect_overrides 无行 ③moves_lib 全 worktree 并集无
//   注册。切片 [异常码,百分比] 12/12 自洽，码表 {0,1}（麻痹/中毒），{1}∈{50,70,100}。
//   纯链 16328 玄天凤坠闪 [1,100]/16540 灭世之力 [0,50]（prio=0，场景用 16328）。
// 先例：J55 652/1204（NextRounds ROUND_START tick 写 ws 字段）、776/500
//   （damage_add_pct 通道）、2158（cure_anomalies 通道互证，J128 进行中）。
// 场景回归：test/scenario/scenario_2390_cure_amp.cpp（2390 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 下回合增伤 tick（NextRounds(1) ROUND_START 桶）：payload = [self, pct]。
EffectResult j141_631_round_start_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[1];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[self] += static_cast<float>(pct);   // 本回合内攻击通吃
    return EffectResult::kOk;
}

}  // namespace

// ── 631 —— 消除{0}状态，消除成功下回合造成伤害提升{1}% ──
// int_args = [owner, target, code, pct]（args_num=2）。
EffectResult effect_631_cure_next_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 631, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 631, args,
                     "631 布局=[owner,target,异常码,提升%]");
    const int self = args.int_args[0];
    const int code = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     631, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 631, args, "需要 提升% > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(code), 631, args, "异常码越界（≤43）");
    if (!g_core || !g_core->cure_anomalies) {
        return EffectResult::kOk;
    }
    const int cured = g_core->cure_anomalies(ctx, self, &code, 1);
    if (cured <= 0) {
        return EffectResult::kOk;   // 消除失败（自身无此状态）：无增伤
    }
    // 下回合增伤：NextRounds(1) ROUND_START tick（本回合不算，J55 窗骨）。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 631,
                                /*rounds=*/1, {self, pct}, &j141_631_round_start_tick);
    return EffectResult::kOk;
}

SKILL_EFFECT(631, effect_631_cure_next_amp)
