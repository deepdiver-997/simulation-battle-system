// J 组 J189 · 体力比例门粉伤/吸取族 —— 2 宏/19 载体（2026-10-22，worktree ../agents/j189）
//
// 家族语义（effect_info 模板即权威）：
//   1136 [2] 自身体力高于1/{0}时附加自身当前体力1/{1}的百分比伤害
//   1137 [2] 自身体力低于1/{0}时吸取对手最大体力的1/{1}
//
// 实现口径：
//   - 1136 = 自身当前体力百分比粉伤：门 = hp > max/{0}（"高于"严格）；
//     数值 = 当前体力 / {1}（模板"自身当前体力1/{1}"，不是最大体力）；
//     通道 = deal_pink_damage(PERCENT_VALUE, 计算值)（j_amp_gates / j_cond_pink_amp 同路）。
//     审计槽 plugin_storage[k1136AuditBase+owner] = 本次粉伤值（精确断言用，无随机）。
//   - 1137 = 低体吸取：门 = hp < max/{0}（"低于"严格）；
//     drain_hp(owner, target, {1})（目标掉 max_hp/{1} 固定伤害、自身回等量；J78 1104 同原语）。
//     审计槽 k1137AuditBase+owner = 本次吸取基数 max_hp_of(target)/{1}。
//   - 两门互为反向比例，故常同链出现（例 [797,1136,1137]）；各自独立判定，可同时不触发。
//   - 时点 = SKILL_EFFECT（默认）：1136 读"当前体力"= 本次攻击落地前的体力（与官方
//     "附加"子句一致）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1136 [门分母,粉伤分母]：30487 万福镇世诀 [2,3]、31151 寂灭终焉咒 [2,3]、
//        34387 锐爪突袭 [2,3]…（10/10 自洽，全部 2/3）
//   1137 [门分母,吸取分母]：26762 弑界之霆 [3,3]、31404 暗神惊梦诀 [2,3]…（9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1136/1137 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5110_hp_ratio_pink_drain.cpp（号段 5110-5119）。

#include "common.h"

namespace {

constexpr int k1136AuditBase = 990113600;   // +owner：本次粉伤值（审计槽）
constexpr int k1137AuditBase = 990113700;   // +owner：本次吸取基数（审计槽）

// ── 1136 —— 自身体力高于1/{0}时附加自身当前体力1/{1}的百分比伤害 ──
// int_args = [owner, target, 门分母, 粉伤分母]（args_num=2）。
EffectResult effect_1136_hp_ratio_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1136, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1136, args,
                     "1136 布局=[owner,target,门分母,粉伤分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int gate_denom = args.int_args[2];
    const int pink_denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1136, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(gate_denom > 0 && pink_denom > 0, 1136, args, "需要 分母 > 0");
    const int cur = ctx->getPet(owner).hp;
    if (cur <= max_hp_of(ctx, owner) / gate_denom) {
        return EffectResult::kOk;   // 门：自身体力高于 1/{0}
    }
    const int pink = cur / pink_denom;
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1136AuditBase + owner] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1137 —— 自身体力低于1/{0}时吸取对手最大体力的1/{1} ──
// int_args = [owner, target, 门分母, 吸取分母]（args_num=2）。
EffectResult effect_1137_low_hp_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1137, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1137, args,
                     "1137 布局=[owner,target,门分母,吸取分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int gate_denom = args.int_args[2];
    const int drain_denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1137, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(gate_denom > 0 && drain_denom > 0, 1137, args, "需要 分母 > 0");
    if (ctx->getPet(owner).hp >= max_hp_of(ctx, owner) / gate_denom) {
        return EffectResult::kOk;   // 门：自身体力低于 1/{0}
    }
    ctx->plugin_storage[k1137AuditBase + owner] = max_hp_of(ctx, target) / drain_denom;
    if (g_core && g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, owner, target, drain_denom);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1136, effect_1136_hp_ratio_pink)
SKILL_EFFECT(1137, effect_1137_low_hp_drain)
