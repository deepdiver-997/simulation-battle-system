// J 组 J192 · 满体力比例粉伤族 —— 1 宏/9 载体（2026-10-22，worktree ../agents/j192）
//
// 家族语义（effect_info 模板即权威）：
//   1273 [1] 若自身满体力则附加自身最大体力1/{0}的百分比伤害
//
// 实现口径（J189 1136 体力比例门同路）：
//   - 门 = 自身当前体力 **等于** 最大体力（"满体力"字面；严格 ==，不吃比例容差）。
//   - 数值 = max_hp / {0}；通道 = deal_pink_damage(PERCENT_VALUE, 计算值)
//     （j_amp_gates / j_cond_pink_amp / j_hp_ratio_pink_drain 同路）。
//   - 时点 = SKILL_EFFECT（默认）：读到的是本次攻击落地前的体力，满血门在本刀前判定。
//   - 审计槽 plugin_storage[k1273AuditBase+owner] = 本次粉伤值（精确断言用，无随机）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1273 [粉伤分母]：34956 魔枝肃杀 [3]（纯链，物 140）、35614 神明谕令 [3]、
//        35943 相知如故 [3]、31478 月劫轮舞 链尾 [3]…（9/9 自洽，全部 3）
//
// 动工前置三查（2026-10-22）：①内核 grep 1273 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5140_fullhp_pink_1273.cpp（号段 5140-5149）。

#include "common.h"

namespace {

constexpr int k1273AuditBase = 990127300;   // +owner：本次粉伤值（审计槽）

// ── 1273 —— 若自身满体力则附加自身最大体力1/{0}的百分比伤害 ──
// int_args = [owner, target, 粉伤分母]（args_num=1）。
EffectResult effect_1273_full_hp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1273, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1273, args,
                     "1273 布局=[owner,target,粉伤分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1273, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1273, args, "需要 分母 > 0");
    const int max_hp = max_hp_of(ctx, owner);
    if (ctx->getPet(owner).hp != max_hp) {
        return EffectResult::kOk;   // 门：自身满体力（严格相等）
    }
    const int pink = max_hp / denom;
    if (pink <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1273AuditBase + owner] = pink;
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1273, effect_1273_full_hp_pink)
