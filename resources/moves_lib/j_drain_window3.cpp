// J 组 J158 · 吸取窗族三 —— 1 宏/10 载体（2026-10-04，worktree ../agents/j158）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   796 [rounds,分母] "{0}回合内每回合吸取对手当前体力的1/{1}"
//     —— ⚠️ **当前体力**口径（对照 597"最大体力"）：drain_hp 原语是最大体力语义
//     不适用 → 手工：fixed_damage(对手, 对手当前 hp/{1}) + heal_amount(自身,
//     实际落地 dealt)（J6 2274/drain_points 口径：按实际扣减回血、吃封回血）。
//     ⚠️ 模板**无"使用技能"字样**（对照 597"每回合使用技能吸取"）→ 无用技门，
//     锚点=J134 1396 同款 1024 首跳范式：施放当回合立即结算 + NextRounds
//     ROUND_START tick（PASS/被控回照跑，记档）。⚠️ 攻击技载体的首跳在
//     SKILL_EFFECT 时点（攻击伤害落地前）——吸取按施放时当前体力，场景实证记档。
//
// 载体切片（unity 库 side_effect.arg_count 切分 10/10 [rounds,分母] 自洽、零剩余）：
//   [4,3]×4、[4,4]×2、[3,3]×2、[1,2]、[2,4]
//   纯链：碎裂晶胞 37669 [2,4]（特140——场景主载体）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 796 无行为硬编码
//   ②custom_effect_overrides 零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J134 1396（首跳范式+无门记档）、J6 drain_points（点数吸取实际扣减口径）、
//   J15 597 同族（最大体力语义对照）。
// 场景回归：test/scenario/scenario_2490_drain_window3.cpp（号段 2490-2499）。

#include "common.h"

namespace {

// ── 796 tick —— 吸对手当前体力 1/{denom}（按实际落地回血）──
EffectResult j158_796_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->fixed_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int foe_hp = ctx->getPet(opp).hp;
    if (foe_hp <= 0) {
        return EffectResult::kOk;   // 对手已倒（495 存活门）
    }
    const int dmg = foe_hp / denom;
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    const int before = foe_hp;
    (void)g_core->fixed_damage(ctx, opp, dmg);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, self, dealt);
    }
    return EffectResult::kOk;
}

// ── 796 —— {0}回合内每回合吸取对手当前体力的1/{1} ──
// int_args = [owner, target, {0}回合, {1}分母]（args_num=2）。
EffectResult effect_796_current_drain_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 796, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 796, args,
                     "796 布局=[owner,target,回合,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 796,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 796, args, "需要 回合/分母 > 0");
    const std::vector<int> payload{self, opp, denom};
    EffectArgs first(payload);
    (void)j158_796_tick(ctx, first);   // 首跳：施放当回合立即结算（无门，记档）
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 796, rounds - 1,
                                    payload, &j158_796_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(796, effect_796_current_drain_window);
