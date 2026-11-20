// J112 · 伤害不足吸取/低体回血反噬族 —— 947×13 / 997×13（2026-10-03，worktree ../agents/j112）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 947（args_num=2）"造成的伤害不足{0}则吸取对手最大体力的1/{1}"：
//   - 阈值门 = AFTER_ACTION 读 resolvedDamage.final（1256/1306 阈值形，J15 729 同款）
//     ——需内核 effect_register_state case 947 归位一行（101/422/628/194 同族）；
//   - "不足" = final < {0}（=={0} 不算）→ drain_hp(自身, 对手, {1})
//     （CoreApi 163：FIXED 伤 1/{1} 对手最大体力 + 按实际落地回吸）。
//   - 实例：血色心眼 19726 [947]|[300,4]（物140 纯链——带 [202,237] 恒 < 300，
//     场景确定性触发）、水灵听风静 19921 [947]|[300,3]。
// 997（args_num=2）"自身体力低于1/{0}时，恢复自身最大体力的1/{1}并造成等量百分比
//   伤害"：
//   - 低体门 = hp < max/{0}（严格小于，1397 口径）；恢复 = heal_amount(max/{1})
//     （吃封回血/恢复修正，common.h 口径）；"等量百分比伤害" = 同额
//     deal_pink_damage(PERCENT_VALUE) 给对手。
//   - 实例：烟火无情 31714 [889,997]|[22,2,4]（特85 prio3；889=消对手回合类已注册，
//     对手空场=良性——J102 同款验证）。
//
// 动工前置三查（2026-10-03）：①内核 grep 947/997 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   947=2/997=2 与 effect_info.args_num 一致；26 载体切片自洽。
// 内核改动：skills.cpp effect_register_state 加 case 947 一行（与并行任务错峰）。
// 先例：1256/1306（AFTER_ACTION 阈值）、J54 628（case 归位族）、drain_hp（CoreApi）、
//   J110 770/J95 1073（回血+等量粉拼装）、1397（低体门）。
// 场景回归：test/scenario/scenario_2060_insufficient_drain_lowheal.cpp（2060 号段）。

#include "common.h"

namespace {

}  // namespace

// ── 947 —— 造成的伤害不足{0}则吸取对手最大体力的1/{1} ──
// 注册时点 = AFTER_ACTION（内核 case 947 归位行）；int_args = [owner,target,阈值,分母]。
EffectResult effect_947_insufficient_damage_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 947, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 947, args,
                     "947 布局=[owner,target,阈值,分母]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 947, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && denom > 0, 947, args, "需要 阈值/分母 > 0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int final_damage = ctx->resolvedDamage.final;
    if (final_damage >= threshold) {
        return EffectResult::kOk;   // "不足{0}"才吸取（=={0} 不算）
    }
    (void)g_core->drain_hp(ctx, self, target, denom);
    return EffectResult::kOk;
}

// ── 997 —— 自身体力低于1/{0}时，恢复自身最大体力的1/{1}并造成等量百分比伤害 ──
// int_args = [owner, target, denom_gate, denom_heal]（args_num=2）。SKILL_EFFECT 直通。
EffectResult effect_997_lowhp_heal_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 997, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 997, args,
                     "997 布局=[owner,target,门分母,恢复分母]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom_gate = args.int_args[2];
    const int denom_heal = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 997, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom_gate > 0 && denom_heal > 0, 997, args, "需要 分母 > 0");
    if (!g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    if (ctx->getPet(self).hp >= max_hp / denom_gate) {
        return EffectResult::kOk;   // 门：自身体力低于 1/{0}（严格小于）
    }
    const int heal = max_hp / denom_heal;
    if (heal <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, self, heal);
    (void)g_core->deal_pink_damage(ctx, target, heal, DamageKind::PERCENT_VALUE, self);
    return EffectResult::kOk;
}

SKILL_EFFECT(947, effect_947_insufficient_damage_drain)
SKILL_EFFECT(997, effect_997_lowhp_heal_reflect)
