// J 组 J207 · 吞噬强化双倍转移族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   569 [0] 吞噬对手的能力提升效果并双倍转移到自己身上
//
// 实现口径：
//   - transfer_stat_boosts(opp → self)：对手正等级整体搬到自身（对手清零、自身等量累加，
//     双方 +6 钳制由原语管；573"被消除或吸取"事件由原语 emit）。
//   - "双倍转移" = 再补一次**本次实际搬到的量**：逐槽 delta = 转移后自身等级 - 转移前，
//     delta>0 → stat_change(self, 槽, delta)（等价于转移量×2；自身原等级不参与加倍）。
//   - 审计槽：plugin_storage[990569100+self*2] = 转移前对手正等级总和（moved_levels）；
//     plugin_storage[990569101+self*2] = 本次实际补加总量（doubled_levels）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   569 []：15444 狂魔霸裂斩 纯链（物 160，场景载体）、19236 无尽灾火 纯链、
//        19418 暗金弥撒 链 [569,583]|[1]、18366 幻妙清风斩 [502,569]|[1,6]…（9 载体全空参）
//
// 动工前置三查（2026-10-22）：①内核 grep 569 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(569)——真缺口。
// 场景回归：test/scenario/scenario_5330_devour_boosts.cpp（号段 5330-5339）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k569MovedBase = 990569100;    // +self*2：转移前对手正等级总和
constexpr int k569DoubledBase = 990569101;  // +self*2：实际补加总量

// ── 569 —— 吞噬对手的能力提升效果并双倍转移到自己身上 ──
// int_args = [owner, target]（args_num=0，loader 注入 owner/target）。
EffectResult effect_569_devour_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 569, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 569, args,
                     "569 布局=[owner,target]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     569, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->transfer_stat_boosts || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    int moved_levels = 0;
    int before[BattleContext::kAbilityLevelSlotCount] = {};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            moved_levels += ctx->ability_levels[opp][i];
        }
        before[i] = ctx->ability_levels[self][i];
    }
    const int moved = g_core->transfer_stat_boosts(ctx, opp, self);
    int doubled = 0;
    if (moved > 0) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            const int delta = ctx->ability_levels[self][i] - before[i];
            if (delta > 0) {
                (void)g_core->stat_change(ctx, self, i, delta);   // 双倍：再补一次
                doubled += delta;
            }
        }
    }
    ctx->plugin_storage[k569MovedBase + self * 2] = moved_levels;
    ctx->plugin_storage[k569DoubledBase + self * 2] = doubled;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(569, effect_569_devour_boosts)
