// J 组 J65 · 状态门固伤族 —— 3 宏/48 载体（2026-10-03，worktree ../agents/j65）
//
// 家族语义（effect_info 模板即权威）：
//   141 [1] 对方处于冻伤状态时，附加{0}点伤害
//   657 [2] 消除{0}状态，消除成功则附加{1}点固定伤害
//   684 [1] 双方任意一方处于能力下降状态均会附加{0}点固定伤害，同时解除自身能力下降状态
//
// 实现口径：
//   - 141 = 冻伤单码门（has_active_anomaly(opp, Frostbite=5)）→ fixed_damage({0})
//     （J63 133/J51 413 门镜像单状态版，SKILL_EFFECT 直通）。
//   - 657 = 定向解除：cure_anomalies(target, [{0}], 1) 返回实际解除条数（按名单
//     选择性版 dispel，544 首用原语）>0 = "消除成功" → fixed_damage({1})。未处于
//     {0} 状态 → 无解除无固伤（成功门槛）。
//   - 684 = 任一负槽门（双侧并集：自身或对手六槽任一 <0，J14 判据）→
//     fixed_damage({0}) + 自身负槽逐槽直改 levels 清零（1221 反转族直改口径；
//     只清自身，对手负槽保留）。
//
// 载体切片（unity 库 json_each 精确匹配 16+16+16 计账；切片全自洽）：
//   141 [固伤]：15492 [100]/12638 [60] 等 10 纯链（链 6 条兄弟 14/33/756/565/842）
//   657 [状态码,固伤]：18255 [0,100] 等 7 纯链（{0} 全为 0=麻痹；链 30003 尾挂
//   697 穿透凭证=内核假阳性族，不入场景）
//   684 [固伤]：16755 [200]/35277 [200] 等 2 纯链
//
// 动工前置三查（2026-10-03）：①内核 grep 141/657/684 无硬编码 ②
// custom_effect_overrides 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1520_status_gate_fixed.cpp（号段 1520-1529）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

// ── 141 —— 对方处于冻伤状态时，附加{0}点伤害 ──
// int_args = [owner, target, amount]（args_num=1）。
EffectResult effect_141_frostbite_gate_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 141, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 141, args,
                     "141 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     141, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 141, args, "需要 固伤 > 0");
    if (!g_core || !g_core->has_active_anomaly || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, target,
                                    static_cast<int>(AbnormalStatusId::Frostbite))) {
        return EffectResult::kOk;   // 门：对方冻伤
    }
    (void)g_core->fixed_damage(ctx, target, amount);
    return EffectResult::kOk;
}

// ── 657 —— 消除{0}状态，消除成功则附加{1}点固定伤害 ──
// int_args = [owner, target, status, amount]（args_num=2）。
EffectResult effect_657_cure_then_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 657, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 657, args,
                     "657 布局=[owner,target,状态码,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int status = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     657, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(status) && amount > 0, 657, args,
                     "需要 合法状态码 + 固伤 > 0");
    if (!g_core || !g_core->cure_anomalies || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int cured =
        g_core->cure_anomalies(ctx, target, &status, 1);   // 按名单解除（544 首用原语）
    if (cured <= 0) {
        return EffectResult::kOk;   // 门：消除成功（未处于该状态则不解不附）
    }
    (void)g_core->fixed_damage(ctx, target, amount);
    return EffectResult::kOk;
}

// ── 684 —— 双方任一方有下降→附加{0}点固伤并解除自身下降 ──
// int_args = [owner, target, amount]（args_num=1）。
EffectResult effect_684_any_drop_fixed_self_clean(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 684, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 684, args,
                     "684 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     684, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 684, args, "需要 固伤 > 0");
    bool any_drop = false;
    for (const int side : {owner, target}) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (ctx->ability_levels[side][i] < 0) {
                any_drop = true;
            }
        }
    }
    if (!any_drop) {
        return EffectResult::kOk;   // 门：双方任一方处于能力下降状态
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, target, amount);
    }
    // 解除自身下降：负槽逐槽直改 levels 清零（1221 反转族直改口径；对手不动）
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] < 0) {
            ctx->ability_levels[owner][i] = 0;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(141, effect_141_frostbite_gate_fixed)
SKILL_EFFECT(657, effect_657_cure_then_fixed)
SKILL_EFFECT(684, effect_684_any_drop_fixed_self_clean)
