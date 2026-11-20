// J 组 J208 · 先手吸取+当回合封攻族 —— 1 宏/10 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   1867 [1] 先出手时吸取对手最大体力的1/{0}且当回合对手无法造成攻击伤害
//
// 实现口径：
//   - SKILL_EFFECT：`acting_side_is_first(ctx)` 门（本回合先手方；J194 1295 同判据）。
//   - 吸取半边 = drain_hp_amount(self, opp, max_hp(opp)/{0})（原语：目标掉固定伤害、
//     actor 恢复等量；走粉伤相与 heal_impl——免粉/封回血各自生效）。
//   - 封攻半边 = RuleCenter DAMAGE 免疫票：grant_immune(self, DAMAGE, duration=1,
//     register=当前回合, source_effect_id=1867)——"当回合"窗口 [R,R+1)，对手本回合
//     后续攻击的 BLOCK 相被免伤票挡下（794 回合窗同型；沧岚抑制门自动覆盖）。
//   - 审计槽 plugin_storage[k1867AuditBase+self] = 本次吸取量（场景精确断言）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1867 [分母]：36689 极涛裂空杀 链 [1810(0参),1867,2033] 段 [4]（物 150；1810/2033
//        未实现→静默，场景链干净）、38277 态度要诚恳 [3]、36963 寒风冰暴 [4]、
//        36712 真目玄元光 [3]…（10/10 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1867 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(1867)——真缺口。
// 场景回归：test/scenario/scenario_5340_first_drain_seal_attack.cpp（号段 5340-5349）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1867AuditBase = 990186700;   // +self：本次吸取量

// ── 1867 —— 先出手时吸取对手最大体力的1/{0}且当回合对手无法造成攻击伤害 ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1867_first_drain_seal_attack(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1867, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1867, args,
                     "1867 布局=[owner,target,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1867, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1867, args, "需要 分母 > 0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 门：本回合先出手
    }
    const int amount = max_hp_of(ctx, opp) / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k1867AuditBase + self] = amount;
    if (g_core && g_core->drain_hp) {
        // drain_hp = 目标掉 max_hp(target)/denom 固定伤害、actor 恢复等量（clamp 到 max_hp）。
        (void)g_core->drain_hp(ctx, self, opp, denom);
    }
    // "当回合对手无法造成攻击伤害"：DAMAGE 免疫票（窗口 [R,R+1)，先手方本回合后续受击生效）。
    ctx->rule_center_.grant_immune(
        self, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
        /*duration_rounds=*/1, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
        EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
        /*source_effect_id=*/1867);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1867, effect_1867_first_drain_seal_attack)
