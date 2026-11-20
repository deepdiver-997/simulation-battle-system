// J 组 J170 · 未暴吸取族 —— 2 宏/24 载体（2026-10-04，worktree ../agents/j170）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   844  [denom] "若攻击未打出致命一击则吸取对手最大体力的1/{0}"
//     —— 暴击门：ctx->crit_happened[owner]（query_usage ①.5 已掷，SKILL_EFFECT 可读
//     ——731 同读点）：未暴 → drain_hp(对手, {0})（597 最大体力语义原语）；
//     暴击 → 无事。单发（模板无窗口字样），SKILL_EFFECT 同点直接结算。
//   1415 [pct,pct_up] "造成伤害的{0}%恢复自身体力，若自身体力低于对手则效果提升至{1}%"
//     —— ⚠️ **与 J137 重复**：feat/j137（DETECT 回吸族）已实现同 id 同语义
//     （j_detect_drain3.cpp effect_1415_damage_heal_low_up，2026-10-04 凌晨交付）。
//     本 worktree 并集核查时点早于 j137 分支合入主仓所致——**记档跳过不产码**
//     （J36 记档跳过先例），合并验收以 feat/j137 版本为准。
//
// 载体切片（unity 库 side_effect.arg_count 切分 12/12 [denom] 自洽、零剩余）：
//   [3]×9、[4]×3——全攻击技载体（伴生全已实现）
//   场景利：斩月落云破 19903 [3]（物150，伴生 1002✓）
//
// 动工前置三查（2026-10-04 复核）：①内核 grep 844 无行为硬编码 ②overrides 零行
//   ③moves_lib 并集无 844 注册——真缺口（1415 见上，重复单记档）。
// 先例：731（crit_happened 读点）、J15 597（drain_hp 最大体力语义）、J23 36（暴击
//   query_usage 时点口径）。
// 场景回归：test/scenario/scenario_2720_crit_miss_drain.cpp（号段 2720-2729）。

#include "common.h"

namespace {

// ── 844 —— 若攻击未打出致命一击则吸取对手最大体力的1/{0} ──
EffectResult effect_844_no_crit_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 844, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 844, args,
                     "844 布局=[owner,target,denom]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 844,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 844, args, "需要 分母 > 0");
    if (ctx->crit_happened[self]) {
        return EffectResult::kOk;   // 打出了致命一击 → 无吸取
    }
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    (void)g_core->drain_hp(ctx, self, opp, denom);   // 597 最大体力语义原语
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(844, effect_844_no_crit_drain);
