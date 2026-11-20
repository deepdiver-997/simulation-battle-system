// J 组 J63 · 状态门输出族二 —— 2 宏/32 载体（2026-10-03，worktree ../agents/j63）
//
// 家族语义（effect_info 模板即权威）：
//   431 [0] 若对手处于能力下降状态，则威力翻倍
//   133 [1] 对方烧伤时，附加{0}点伤害
//
// 实现口径：
//   - 431 = 负槽判据（六槽 ability_levels[opp]<0 任一，j51_count_boost_kinds 取反门）
//     → 威力视图 ×2（1443 同通道：ws.skill_power_view 重写 operator= 置 rewritten →
//     标准变威力重算管线；1103 门镜像的威力版）。零参：int_args=[owner,target]。
//   - 133 = 烧伤单码门（g_core->has_active_anomaly(opp, Burn)）→ fixed_damage({0})
//     （413 门镜像的单状态版；粉伤 FIXED 档）。场景断言口径见场景文件头：
//     烧伤回合末 tick 污染 hp 差分 → 用同对手两轮（拍打/载体）的 (loss-红) 差分恰为
//     {0}，tick 两轮相等相消。
//
// 载体切片（unity 库 json_each 精确匹配 16+16 计账；切片全自洽）：
//   431 零参切片全空：13579（特90）/15891（特160）/17251（物140）等 6 纯链
//   133 [固伤] 单参：12411 [100]/12763 [50]/12855 [60]/12938 [150] 等 12 纯链
//
// 动工前置三查（2026-10-03）：①内核 grep 431/133 无硬编码 ②custom_effect_overrides
// 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1490_status_gate_output2.cpp（号段 1490-1499）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

// 对手负槽判据：六槽任一 <0（"处于能力下降状态"；j51 正槽计数的镜像）。
inline bool j63_foe_dropped(const BattleContext* ctx, int target) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[target][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 431 —— 若对手处于能力下降状态，则威力翻倍 ──
// int_args = [owner, target]（args_num=0，仅前置绑定两位）。
EffectResult effect_431_drop_gate_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 431, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 431, args,
                     "431 零参布局=[owner,target]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     431, args, "需要 owner/target ∈ {0,1}");
    if (!j63_foe_dropped(ctx, target)) {
        return EffectResult::kOk;   // 门：对手处于能力下降状态
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;   // 翻倍（置 rewritten → 标准变威力重算管线，1443 同路）
    }
    return EffectResult::kOk;
}

// ── 133 —— 对方烧伤时，附加{0}点伤害 ──
// int_args = [owner, target, amount]（args_num=1）。
EffectResult effect_133_burn_gate_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 133, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 133, args,
                     "133 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     133, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 133, args, "需要 固伤 > 0");
    if (!g_core || !g_core->has_active_anomaly || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, target, static_cast<int>(AbnormalStatusId::Burn))) {
        return EffectResult::kOk;   // 门：对方烧伤
    }
    (void)g_core->fixed_damage(ctx, target, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(431, effect_431_drop_gate_power_double)
SKILL_EFFECT(133, effect_133_burn_gate_fixed)
