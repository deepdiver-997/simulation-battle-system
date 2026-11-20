// J 组 J39 · 条件护盾族二 —— 3 宏/59 载体（2026-10-02，worktree ../agents/j39）
//
// 家族语义（effect_info 模板即权威）：
//   960  [3] {0}回合内若自身处于能力提升状态则每回合使用技能{1}%令对手{2}
//   1013 [2] 自身每处于{0}种能力提升状态，则为自身附加{1}点护盾
//   1451 [2] 若自身体力高于对手则获得{0}点护盾，若自身体力低于对手则恢复自身最大体力的1/{1}
//
// 实现口径：
//   - 960 = 597 窗（AFTER_ACTION 双侧注册，InRounds 含当回合）+ tick 时判"自身有正槽"
//     （count_boost_kinds 同判据——持续条件在 tick 求值，非施放时）+ J3 申报制
//     apply_anomaly_checked。960 模板无"攻击"字样 → 不限攻击技。
//   - 1013 = count_boost_kinds（六槽正槽计数）÷{0} ×{1} 点 shield_bank_.add
//     （priority=0，2005/1869 直系）。"每{0}种"= 整除计组。
//   - 1451 = hp 对比双分支（对比**对手在场宠**当前 hp）：高于 → shield {0}；
//     低于 → heal_max_hp_fraction(1/{1})；相等无事（模板无相等分支，记档）。
//
// 载体切片（unity 库 json_each 精确匹配 21+19+19=59；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   960  [rounds,chance,异常码]：28224 怒火长缨 [3,100,22]（chance=100 测试载体）
//   1013 [种数,点数]：28938 重装上阵 [1,160]、33173 追踪导弹 [1,50]
//   1451 [盾点,分母]：27293 源矿吸收 [300,3]、28735 星光·花草能量 [400,2]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1240_cond_shield2.cpp（号段 1240-1249）。

#include "common.h"

namespace {

// 自身正槽计数（batch_a/batch_c count_boost_kinds 同判据：六槽含命中）。
inline int j39_count_boost_kinds(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 960 的 AFTER_ACTION tick —— payload={owner, target, chance, anomaly}。
EffectResult j39_960_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 持续条件：tick 时自身处于能力提升状态（有正槽）。
    if (j39_count_boost_kinds(ctx, owner) <= 0) {
        return EffectResult::kOk;
    }
    if (!used_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly_checked) {
        (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                            owner, chance, ChanceSource::Skill);
    }
    return EffectResult::kOk;
}

// ── 960 —— {0}回合内若自身处于能力提升状态则每回合使用技能{1}%令对手{2} ──
// int_args = [owner, target, rounds, chance, anomaly]（args_num=3）。
EffectResult effect_960_boost_gate_skill_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 960, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 960, args,
                     "960 布局=[owner,target,回合,概率,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     960, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0, 960, args, "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 960, args, "需要 异常码 ∈ [0,43]");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 960, rounds, {owner, target, chance, anomaly},
                              &j39_960_round_tick);
    }
    return EffectResult::kOk;
}

// ── 1013 —— 自身每处于{0}种能力提升状态，则为自身附加{1}点护盾 ──
// int_args = [owner, target, kinds_per, points]（args_num=2）。
EffectResult effect_1013_boost_kinds_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1013, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1013, args,
                     "1013 布局=[owner,target,种数,点数]");
    const int owner = args.int_args[0];
    const int kinds_per = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1013, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(kinds_per > 0 && points > 0, 1013, args, "需要 种数/点数 > 0");
    const int kinds = j39_count_boost_kinds(ctx, owner);
    const int groups = kinds / kinds_per;   // "每{0}种"= 整除计组（不足一组不附）
    if (groups <= 0) {
        return EffectResult::kOk;
    }
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, groups * points, /*source_id=*/1013,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1451 —— 若自身体力高于对手则获得{0}点护盾，若自身体力低于对手则恢复自身最大体力的1/{1} ──
// int_args = [owner, target, shield, denom]（args_num=2）。
EffectResult effect_1451_hp_compare_shield_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1451, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1451, args,
                     "1451 布局=[owner,target,盾点,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int shield = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1451, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(shield > 0 && denom > 0, 1451, args, "需要 盾点/分母 > 0");
    const int my_hp = ctx->getPet(owner).hp;
    const int opp_hp = ctx->getPet(opp).hp;
    if (my_hp > opp_hp) {
        ctx->getPet(owner).shield_bank_.add(/*priority=*/0, shield, /*source_id=*/1451,
                                            /*duration=*/0);
        return EffectResult::kOk;
    }
    if (my_hp < opp_hp) {
        (void)heal_max_hp_fraction(ctx, owner, denom);   // 恢复自身最大体力 1/{1}
    }
    // 相等：模板无分支，无事发生（记档）。
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(960, effect_960_boost_gate_skill_anomaly)
SKILL_EFFECT(1013, effect_1013_boost_kinds_shield)
SKILL_EFFECT(1451, effect_1451_hp_compare_shield_heal)
