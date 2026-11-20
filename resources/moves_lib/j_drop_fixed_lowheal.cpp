// J 组 J96 · 降槽固伤/低体翻倍回血族 —— 542×14 / 900×14（2026-10-03，worktree ../agents/j96）
//
// 家族语义（effect_info 官方模板即权威）：
//   542（arg_count=1）"对手每处于一种能力下降状态时附加{0}点固定伤害"
//   900（arg_count=2）"恢复自身最大体力的1/{0}，体力少于1/{1}时恢复效果翻倍"
//
// 实现口径：
//   - 542 = 负槽计数 × {0} → fixed_damage（808 镜像：正槽计数版在 J27；计数循环
//     ability_levels[opp][i] < 0，burn_trio/crawford 同款）。SKILL_EFFECT 直通。
//   - 900 = heal 量 = max_hp/{0}；低体门（hp×{1} < max，1397 整数直乘口径）成立时
//     ×2（J40 708 "低体翻倍"同族）→ heal_amount 直呼（精确点数控制，不走分数助手）。
//     SKILL_EFFECT 直通。
//
// 载体切片（unity 库 json_each 精确匹配 14/14=28；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   542 [固伤/槽]：{80} 等（纯链 千煞孤魔断 19778 [80] 物150 / 灰岩锁链 35585 [80] 特140）
//   900 [分母,低体分母]：分散（场景载体 影幻之斩 19356 [635✗,900]|[3,4,3] 物85——
//     635 未实现哑；slice [3,4] → 1/3 回血、<1/4 翻倍）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 542/900 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：808（计数×固伤，J27）、708/1397（低体翻倍+整数直乘，J40/J17）、
//   heal_amount 直呼点数控制。
// 场景回归：test/scenario/scenario_1890_drop_fixed_lowheal.cpp（1890 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 542 —— 对手每处于一种能力下降状态时附加{0}点固定伤害 ──
// int_args = [owner, target, per_slot]（args_num=1）。
EffectResult effect_542_drop_count_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 542, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 542, args,
                     "542 布局=[owner,target,每槽固伤]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int per_slot = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     542, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(per_slot > 0, 542, args, "需要 每槽固伤 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    int drops = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            ++drops;
        }
    }
    if (drops > 0) {
        (void)g_core->fixed_damage(ctx, opp, drops * per_slot);
    }
    return EffectResult::kOk;
}

// ── 900 —— 恢复自身最大体力的1/{0}，体力少于1/{1}时恢复效果翻倍 ──
// int_args = [owner, target, denom, low_denom]（args_num=2）。
EffectResult effect_900_lowhp_double_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 900, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 900, args,
                     "900 布局=[owner,target,分母,低体分母]");
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 900, args, "owner 越界");
    EFFECT_ARG_GUARD(denom > 0 && low_denom > 0, 900, args, "需要 分母/低体分母 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int base = max_hp_of(ctx, self) / denom;
    if (base <= 0) {
        return EffectResult::kOk;
    }
    const int amount = (ctx->getPet(self).hp * low_denom < max_hp_of(ctx, self))
                           ? base * 2   // 体力少于 1/{1} → 翻倍（1397 整数直乘口径）
                           : base;
    (void)g_core->heal_amount(ctx, self, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(542, effect_542_drop_count_fixed)
SKILL_EFFECT(900, effect_900_lowhp_double_heal)
