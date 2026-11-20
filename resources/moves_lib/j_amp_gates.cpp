// J 组 J172 · 增伤门族 —— 903×12 / 631×12（2026-10-04，worktree ../agents/j172）
//
// 家族语义（effect_info 官方模板即权威）：
//   903 [1] 自身体力高于对手时附加自身最大体力值{0}%的百分比伤害
//   631 [2] 消除{0}状态，消除成功下回合造成伤害提升{1}%
//
// 实现口径：
//   - 903 = SKILL_EFFECT hp 对比门（self > opp **严格高于**，970/836 同判据）→
//     deal_pink_damage(opp, max_hp_of(self)×{0}%, PERCENT_VALUE)——⚠️ 基数是
//     **自身最大体力**（非攻击面板，与 944 双防和/1163 攻击和的基数族区分）。
//   - 631 = has_active_anomaly(self,{0}) 门 → set_abnormal_status_end_round 压尽
//     （J105 629 CoreApi 消除形，目标=自身）成功 → NextRounds(1) ROUND_START tick
//     写 ws.damage_add_pct[self] 首空槽 +{1}。⚠️ F6 记档的正确用法：
//     damage_add_pct[X] = **X 造成伤害** 的 AMP 通道（bucket_owner==attackerId）——
//     "下回合【自身】造成伤害提升"正是 self-side，首空槽写法同 817 受击侧镜像。
//
// 载体切片（unity 库 json_each 精确匹配 12+12=24；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   903 [pct]：{30}×6/{25}×4/{20}×2——纯链×2（笔走龙蛇 30005 [30] 物 / 36540 [30] 特）
//   631  [码,pct]：{0,100}/{1,100}/{0,50}/… 纯链×4（玄天凤坠闪 16328 [1,100] 物、
//     灭世之力 16540 [0,50] 特——⚠️ 码 0 载体场景避用（J97 非确定记档），场景用 16328）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 2 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：deal_pink_damage PERCENT_VALUE（944/1697）、max_hp_of（common.h）、
//   629 消除形（j_cleanse_prio_reversal_pp）、damage_add_pct 首空槽（J20/J3 693 形）、
//   register_next_rounds_effect ROUND_START（696 重写器形）。
// 场景回归：test/scenario/scenario_2740_amp_gates.cpp（2740 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 903 —— 自身体力高于对手时附加自身最大体力值{0}%的百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_903_hphigher_maxhp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 903, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 903, args,
                     "903 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     903, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 903, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // "高于"门（严格大于）
    }
    const int pink = max_hp_of(ctx, owner) * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 631 tick：下回合自身造成伤害 +{1}%（damage_add_pct 首空槽）──
EffectResult j172_631_amp_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int amp = a.int_args[1];
    if (self < 0 || self > 1 || amp == 0) {
        return EffectResult::kOk;
    }
    c->ws.damage_add_pct[self] += static_cast<float>(amp);   // 标量累加器（AMP 求和通道）
    return EffectResult::kOk;
}

// ── 631 —— 消除{0}状态，消除成功下回合造成伤害提升{1}% ──
// int_args = [owner, target, anomaly, amp]（args_num=2）。
EffectResult effect_631_cleanse_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 631, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 631, args,
                     "631 布局=[owner,target,状态码,增伤]");
    const int owner = args.int_args[0];
    const int anomaly = args.int_args[2];
    const int amp = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     631, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 631, args,
                     "需要 状态码 ∈ [0,43]");
    EFFECT_ARG_GUARD(amp > 0, 631, args, "需要 增伤 > 0");
    if (!g_core || !g_core->has_active_anomaly || !g_core->set_abnormal_status_end_round) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, owner, anomaly)) {
        return EffectResult::kOk;   // "消除成功"门槛：自身不处于{0}状态
    }
    g_core->set_abnormal_status_end_round(ctx, owner, anomaly, ctx->roundCount);
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 631,
                                /*rounds=*/1, {owner, amp}, &j172_631_amp_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(903, effect_903_hphigher_maxhp_pink)
SKILL_EFFECT(631, effect_631_cleanse_amp)
