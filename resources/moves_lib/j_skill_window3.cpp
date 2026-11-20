// J 组 J134 · 用技吸窗族三 —— 3 宏/38 载体（2026-10-04，worktree ../agents/j134）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1462 [rounds,固伤]   "{0}回合内每回合使用技能附加{1}点固定伤害，附加时若自身
//     不处于能力提升状态则固定伤害翻倍" —— 成功用技门（executed_skill_this_round，
//     2026-10-03 判据统一 §四点五：使用=成功使用出来，被控不算——J6 时代旧码
//     used_skill_this_round 系选择位语义，本单不沿用，记档）；翻倍在结算前折算
//     （count_boost_kinds(self)==0 → 2×{1}，"附加时"即 tick 时点动态判）。
//   1927 [rounds,点数,分母] "{0}回合内每回合使用技能则吸取对手{1}点体力，吸取体力时
//     若自身体力低于最大体力的1/{2}则吸取效果翻倍" —— 点数吸血（J6 2274 口径：
//     fixed_damage 走固伤通道、按**实际扣减**回血、回血吃封回血）；低体门
//     hp < max_hp/{2} → 2×{1}。
//   1396 [rounds,分母1,分母2,分母3] "{0}回合内每回合吸取对手最大体力的1/{1}，
//     自身体力低于最大体力的1/{2}时转变为吸取对手最大体力的1/{3}" —— ⚠️ 模板
//     **无"使用技能"字样**（对照 597"每回合使用技能吸取"）→ 无用技门；⚠️ 锚点
//     记档：AFTER_ACTION 桶在 owner NONE/被控回合被 should_skip_action_flow 整段
//     跳过（battleFsm:1815/1981）——"每回合"字面要求 PASS 回合照常结算，故取
//     **1024 首跳范式**：用技当回合立即结算一次 + NextRounds ROUND_START tick
//     （2..{0} 回合，PASS/被控回照跑；J14 1024 同款切分记档）；低体转变=窗口内
//     **动态判**（工单口径，791 低体门同构）：每 tick 按 hp<max/{2} 在 1/{1} 与
//     1/{3} 间就地取分母（载体 [4,3,2,2] 族低体转 1/2=更狠，方向自洽）。
//
// 实现口径（J6 j_round_heal_drain.cpp H8 范式直系）：
//   FIRST/SECOND_AFTER_ACTION 双侧注册（InRounds rounds={0}；引擎按
//   execute_registered_actions(mover_id, bucket) 只在**自己**的 after-action 结算——
//   每回合恰一发，无双发）；首回合当次即结算（AFTER_ACTION 晚于 SKILL_EFFECT 注册点）。
//   drain_hp(actor=self,target=opp,denom)：对手掉 max_hp/denom 固定伤、自身回等量。
//
// 载体切片（unity 库 side_effect.arg_count 切分 13+12+13=38 全载体自洽、零剩余）：
//   1462 [回合,固伤] ×13：[3,200]×5、[3,250]×2、[4,200]×2、[3,240]、[3,300]、
//     [3,150]、[4,300]；纯链：落英缤纷 26681 [3,200]（cat4）
//   1927 [回合,点数,分母] ×12：[4,300,2]×4、[4,250,2]×3、[4,350,2]、[4,280,2]、
//     [4,400,2]、[4,404,2]、[3,250,2]；纯链：浴血杀阵 28223 [4,300,2]（cat4）
//   1396 [回合,分母1,分母2,分母3] ×13：[4,3,2,2]×11、[4,3,3,2]、[3,4,4,3]；
//     纯链：黯辰陨陆 26415 [4,3,2,2]（cat4）
//
// 动工前置三查（2026-10-04）：①内核 grep 3 id 无硬编码 ②custom_effect_overrides
//   零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J6 791/1842/1836（用技窗+低体门+转变读法）、J42 2274（点数吸血实际扣减口径）、
//   判据统一 §四点五（executed_* 助手强制）。
// 场景回归：test/scenario/scenario_2290_skill_window3.cpp（号段 2290-2299，
//   三纯链载体精确断言）。

#include "common.h"

namespace {

// 正槽种类数（batch_a/batch_c 同判据：含命中 6 槽，>0 计一种）。
int j134_boost_kinds(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// 点数吸血（J6 2274 口径：固伤通道 + 按实际扣减回血）。
int j134_drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return 0;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
    return dealt;
}

// ── 1462 tick —— 成功用技 → {1} 固伤（无提升翻倍，附加时动态判）──
EffectResult j134_1462_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // 成功用技门（被控/NONE 不算）
    }
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int pts = j134_boost_kinds(ctx, self) > 0 ? fixed : fixed * 2;
    (void)g_core->fixed_damage(ctx, opp, pts);
    return EffectResult::kOk;
}

// ── 1462 —— {0}回合内每回合使用技能附加{1}点固定伤害（无提升翻倍）──
EffectResult effect_1462_skill_fixed_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1462, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1462, args,
                     "1462 布局=[owner,target,回合,固伤]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1462,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && fixed > 0, 1462, args, "需要 回合/固伤 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1462, rounds, {self, opp, fixed},
                              &j134_1462_tick);
    }
    return EffectResult::kOk;
}

// ── 1927 tick —— 成功用技 → 吸 {1} 点（低体 1/{2} 翻倍，吸取时动态判）──
EffectResult j134_1927_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    const int denom = args.int_args[3];
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const bool low = denom > 0 && ctx->getPet(self).hp * denom < max_hp;
    (void)j134_drain_points(ctx, self, opp, low ? points * 2 : points);
    return EffectResult::kOk;
}

// ── 1927 —— {0}回合内每回合使用技能吸取{1}点（低体 1/{2} 翻倍）──
EffectResult effect_1927_skill_drain_low_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1927, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1927, args,
                     "1927 布局=[owner,target,回合,点数,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1927,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && points > 0 && denom > 0, 1927, args,
                     "需要 回合/点数/分母 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1927, rounds, {self, opp, points, denom},
                              &j134_1927_tick);
    }
    return EffectResult::kOk;
}

// ── 1396 tick —— 每回合吸取 1/{denom1}；低体 1/{denom2} → 转 1/{denom3}（动态判，
//    无用技门——模板无"使用技能"字样，文件头记档）──
EffectResult j134_1396_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom1 = args.int_args[2];
    const int denom2 = args.int_args[3];
    const int denom3 = args.int_args[4];
    const int max_hp = max_hp_of(ctx, self);
    const bool low = denom2 > 0 && ctx->getPet(self).hp * denom2 < max_hp;
    const int denom = low ? denom3 : denom1;
    if (denom > 0) {
        (void)g_core->drain_hp(ctx, self, opp, denom);
    }
    return EffectResult::kOk;
}

// ── 1396 —— {0}回合内每回合吸取 1/{1}，低体 1/{2} 转 1/{3} ──
// 锚点=1024 首跳范式（文件头记档）：当回合立即结算 + NextRounds ROUND_START tick。
EffectResult effect_1396_round_drain_shift(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1396, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1396, args,
                     "1396 布局=[owner,target,回合,分母1,分母2,分母3]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1396,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && args.int_args[3] > 0 && args.int_args[4] > 0
                         && args.int_args[5] > 0,
                     1396, args, "需要 回合/分母 > 0");
    const std::vector<int> payload{self, opp, args.int_args[3], args.int_args[4],
                                   args.int_args[5]};
    EffectArgs first(payload);
    (void)j134_1396_tick(ctx, first);   // 首跳：用技当回合立即结算（含当回合口径）
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1396, rounds - 1,
                                    payload, &j134_1396_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1462, effect_1462_skill_fixed_double);
SKILL_EFFECT(1927, effect_1927_skill_drain_low_double);
SKILL_EFFECT(1396, effect_1396_round_drain_shift);
