// J 组 J156 · 反转/免百分比杂门 —— 551×11 / 1885×11 / 970×11（2026-10-04，worktree ../agents/j156）
//
// 家族语义（effect_info 官方模板即权威）：
//   551  [0] 使对手下回合受到其他技能的伤害翻倍
//   1885 [3] 吸取对手最大体力的1/{0}，若对手免疫百分比伤害则{1}%令对手{2}
//   970  [1] 双倍反转对手能力提升状态，反转成功则下回合自身所有技能先制+{0}，
//        反转失败则消除对手能力提升状态
//
// 实现口径：
//   - 551 = NextRounds(1) ROUND_START tick → ws.damage_reduce_add[opp] 首空槽写 **-100**
//     （⚠️ 看板行原路线 damage_add_pct[对手] 与 F6 817 记档的通道语义相悖：
//     damage_add_pct[X] 是 **X 造成伤害** 的 AMP 通道（bucket_owner==attackerId 才生效），
//     写对手会把官方语义反转成"对手造成伤害翻倍"；"受到伤害翻倍"必须走**受击侧**
//     damage_reduce_add 负值（REDUCE_PCT add 钳底 -100=×2），817 直系修正）。
//     ws 每回合 reset → 窗 tick 只写 R2 一轮，天然"下回合"限定。
//   - 1885 = drain_hp(1/{0})（597 口径，FIXED 通道）+ ctx->is_immune(opp,
//     ImmunityType::PINK_DAMAGE, currentState) → apply_anomaly_checked({1}% 码{2})
//     （申报制；免粉检测=1697 同款面。记档：drain_hp 走 FIXED 通道、免百分比并不
//     真正挡住该吸取，子句按字面作为补偿 rider 落地）。
//   - 970 = 快照对手正等级 → stat_boost_reversal(opp)（978 快照/补给直系）：
//     REVERSED → 逐槽 stat_change(self, i, ×2)（"双倍反转"）+ NextRounds(1)
//     FIRST_MOVE_RIGHT tick preemptive+={0}（J133 635 正位通道）；
//     NOTHING/BLOCKED → clear_stat_boosts(opp)（消除提升，可被免弱挡下=记档）。
//
// 载体切片（unity 库 json_each 精确匹配 11+11+11=33；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-04）：
//   551  []（零参，翻倍写死文案）纯链×3：闪光聚能斧 15524 物 / 圣战女神焰 15955 特 /
//     36947 特（全 pri0）
//   1885 [分母,概率,码]：{3,100,6}/{2,100,6}/{4,100,15}/{3,100,7}/… {1} 全 100——
//     无纯链；场景载体 苦海慈航 28157 cat4（链 499/191/1766✓ 均不扰 hp 断言）
//   970  [先制]：{1}×4/{3}×2/… 无纯链——场景载体 欺罔之视 28305 [3] pri0 cat4
//     （链 1338✓ 只加自身强化、1438 哑，均不扰先制读数）
//
// 动工前置三查（2026-10-04 本 worktree 复核，含 CARRY_EFFECT）：①内核 grep 3 id
// 无硬编码 case ②effect_meta 无行 ③moves_lib 并集无注册——真缺口。
// 先例：817（damage_reduce_add 负值=受伤翻倍，f6）、1697（is_immune PINK_DAMAGE，
// j_pink_drain）、597（drain_hp 口径）、978（反转快照/补给，j_purge_roundly）、
//   clear_stat_boosts（CoreApi 63）、635/1726（NextRounds FIRST_MOVE_RIGHT 先制 tick）。
// 场景回归：test/scenario/scenario_2580_reversal_misc_gates.cpp（2580 号段）。

#include "common.h"

namespace {

// ── 551 tick：下回合对手受伤翻倍（damage_reduce_add 首空槽 -100）──
EffectResult j156_551_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int marked = a.int_args[0];
    if (marked < 0 || marked > 1) {
        return EffectResult::kOk;
    }
    int (&slots)[4] = c->ws.damage_reduce_add[marked];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = -100;   // add 钳底 -100 = ×2（817 同款）
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 551 —— 使对手下回合受到其他技能的伤害翻倍（零参）──
EffectResult effect_551_vulnerable_next(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 551, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 551, args,
                     "551 布局=[owner,target]（on_selected 绑定）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 551, args, "需要 owner ∈ {0,1}");
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, 1 - owner, 551,
                                /*rounds=*/1, {1 - owner}, &j156_551_tick);
    return EffectResult::kOk;
}

// ── 1885 —— 吸取 1/{0}；对手免百分比 → {1}% 令对手{2} ──
// int_args = [owner, target, denom, chance, anomaly]（args_num=3）。
EffectResult effect_1885_drain_pinkimmune_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1885, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1885, args,
                     "1885 布局=[owner,target,分母,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1885, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && chance > 0, 1885, args, "需要 分母/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1885, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->drain_hp || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    (void)g_core->drain_hp(ctx, owner, opp, denom);
    if (ctx->is_immune(opp, ImmunityType::PINK_DAMAGE, ctx->currentState)
        && is_valid_abnormal_status_id(anomaly)) {
        (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                            owner, chance, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 970 —— 双倍反转；成功→下回合先制+{0}；失败→消除对手提升 ──
// int_args = [owner, target, prio]（args_num=1）。
EffectResult effect_970_double_reversal_prio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 970, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 970, args,
                     "970 布局=[owner,target,先制]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int prio = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     970, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(prio > 0, 970, args, "需要 先制 > 0");
    if (!g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    int boosted[BattleContext::kAbilityLevelSlotCount]{};
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        boosted[i] = std::max(0, ctx->ability_levels[opp][i]);
    }
    if (g_core->stat_boost_reversal(ctx, opp) != StatReversalResult::REVERSED) {
        // 反转失败 → 消除对手能力提升（免弱可挡下；无可反时为 no-op）
        if (g_core->clear_stat_boosts) {
            (void)g_core->clear_stat_boosts(ctx, opp);
        }
        return EffectResult::kOk;
    }
    if (g_core->stat_change) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (boosted[i] > 0) {
                g_core->stat_change(ctx, owner, i, boosted[i] * 2);   // "双倍反转"
            }
        }
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 970,
                                /*rounds=*/1, {owner, 1 - owner, prio},
                                &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(551, effect_551_vulnerable_next)
SKILL_EFFECT(1885, effect_1885_drain_pinkimmune_anomaly)
SKILL_EFFECT(970, effect_970_double_reversal_prio)
