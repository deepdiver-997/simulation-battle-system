#include <algorithm>
// J 组 J77 · 低血门/双倍反转族 —— 2 宏/30 载体（2026-10-03，worktree ../agents/j77）
//
// 家族语义（effect_info 模板即权威）：
//   1015 [1] 自身体力低于对手时恢复自身最大体力的1/{0}
//   896  [0] 双倍反转自身的能力下降
//
// 实现口径：
//   - 1015 = 体力比较门（getPet(owner).hp < getPet(target).hp）→
//     heal_max_hp_fraction(owner,{0})（J50 579 同款回血原语）。SKILL_EFFECT 直通。
//   - 896 = 负槽逐槽直改 levels：lv<0 → new = -2×lv（正值两倍幅度；1221 直改口径；
//     记档：官方「反转」语义不走 stat_drop、无免弱面——stat_boost_reversal 原语是
//     1:1 反转且只挂事件，双倍幅度需直改）。零参：int_args=[owner,target]。
//
// 载体切片（unity 库 json_each 精确匹配 15+15 计账；切片全自洽）：
//   1015 [分母]：纯链 27919 兽纹威颜 [3]（属性技）/28336 [2]/30263 [3]/34160 [3]/35716 [3]
//   896  零参切片全空：纯链 30788 混同神力（特130）/32698/32802/32969/33159/33507/
//        33853/36948 全攻击技（19317 [697,896] 链尾为穿透凭证族，不入场景）
//
// 动工前置三查（2026-10-03）：①内核 grep 1015/896 无硬编码 ②custom_effect_overrides
// 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1670_lowhp_double_reversal.cpp（号段 1670-1679）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

// ── 1015 —— 自身体力低于对手时恢复自身最大体力的1/{0} ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_1015_lower_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1015, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1015, args,
                     "1015 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1015, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1015, args, "需要 分母 > 0");
    if (ctx->getPet(owner).hp >= ctx->getPet(target).hp) {
        return EffectResult::kOk;   // 门：自身体力低于对手
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

// ── 896 —— 双倍反转自身的能力下降（零参）──
// int_args = [owner, target]（args_num=0）。
EffectResult effect_896_double_reversal_self(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 896, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 896, args,
                     "896 零参布局=[owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 896, args, "需要 owner ∈ {0,1}");
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[owner][i];
        if (lv < 0) {
            // ⚠️ 钳到 +6（官方等级域）：lv=-4 → +8 越界会炸读端数值（night fuzz #9 家族）。
            //   官方"双倍反转"语义 = 等级域内翻倍，到顶即停。
            ctx->ability_levels[owner][i] = std::min(6, -2 * lv);
            ctx->ws.view_levels[owner][i] = ctx->ability_levels[owner][i];
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1015, effect_1015_lower_heal)
SKILL_EFFECT(896, effect_896_double_reversal_self)
