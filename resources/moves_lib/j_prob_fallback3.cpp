// J 组 J14 · 概率未触发族三 —— 5 宏/113 载体（2026-10-02，worktree ../agents/j14）
//
// 家族语义（effect_info 模板即权威，主干全部"概率施加异常"+ 条件/兜底变体）：
//   966  [3] {0}%的概率使对手{1}，未触发则自身全属性+{2}
//   1274 [2] {0}%令对手{1}，对手处于能力下降状态时触发概率翻倍
//   1248 [2] 对手处于异常状态时{0}%令对手{1}
//   1488 [2] 遇到天敌时{0}%令对手{1}
//   1024 [3] {0}回合内{1}%的概率免疫对手攻击伤害，未触发则回合结束时附加{2}%伤害量的百分比伤害
//
// 实现口径：
//   - 主干 = apply_anomaly_checked 申报制（916/J3 口径）；"未触发" = !anomaly_landed
//     （含 ROLL_FAILED 与免疫/抗性/弹控各被挡档）。
//   - 1274 翻倍 = min(2×{0}, 100)；门槛 = 对手任一负槽（ability_levels 六槽含命中，
//     J13 同判据）。⚠️ 申报概率写 **审计槽** plugin_storage[990127410+owner]（F9 782
//     口径）——翻倍是概率重写，场景借此做无随机精确断言。
//   - 1248 门槛 = has_any_active_abnormal_status(opp)（2009/1048 同判据，BattleContext 内联）。
//   - 1488 门槛 = is_predator(owner)（common.h：对手固有属性克制自身固有属性，不看技能系别）。
//   - 966 兜底 = all_stats_up（六槽含命中；自升走 stat_change 不吃免弱）。
//   - 1024 = 850 概率免票骨架 + 兜底粉伤 tick：
//       · 首回合用技即掷（850 首跳先例：ROUND_START 本回合已跑过）；2..{0} 回合走
//         NextRounds 的 ROUND_START tick 每回合掷一次；
//       · 掷中 → rule_center_.grant_immune DAMAGE duration=1（850 同款"当回合"票）；
//       · 掷歪 → 当回合 ROUND_END 一次性兜底：附加 {2}% × **当回合实际受伤量** 的粉伤
//         （4399 四载体逐字"未触发免疫则在回合结束时附加{2}%伤害量的固定伤害"——
//         伤害量 = 掷点时刻到 ROUND_END 的自身 hp 差；掷中回合不结算）。
//       · ⚠️ 伤害类别记档：effect_info 模板写"百分比伤害"，4399 载体（冥界王权/断魂深渊/
//         暗夜闪现/重装铠甲）一致写"**固定**伤害" → 取 DamageKind::FIXED（1727/1278
//         "X%伤害量反弹"同款判例）。
//
// 载体切片（unity 库 json_each 精确匹配 24+22+22+21+24=113；side_effect.arg_count
// 切分全载体自洽、零剩余，脚本核算 2026-10-02）：
//   966  [chance,anomaly,up]：28015 酱丸启航 [100,2,1]、33854 噬炎鞭笞 [100,2,1]（chance=100）
//   1274 [chance,anomaly]：28395 常黯之空 [50,6]、33679 万伏之触 [40,0]
//   1248 [chance,anomaly]：28603 万物绘卷 [100,1]、28676 光耀坚铠 [100,6]（chance=100）
//   1488 [chance,anomaly]：28864 妖异啸风 [100,10]、35196 雪梅之傲 [100,5]（chance=100）
//   1024 [rounds,chance,pct]：25053 冥界王权 [3,50,100]、25378 秩序守恒 [3,70,120]、
//        29427 天地人磁场 [3,50,150]、27601 麦田守护 [3,50,50]
//
// 动工前置三查（2026-10-02）：①内核 grep 5 id 无行为硬编码 ②custom_effect_programs/
// overrides 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_990_prob_fallback3.cpp（号段 990-999）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 1024 的 ROUND_END 兜底节点伪 id（桶键，10^9 段外）；1274 审计槽基址（F9 782 口径）。
constexpr int kJ14Node1024Fallback = 990102401;
constexpr int kAudit1274Base = 990127410;   // +owner

// 现代施加的"落地"判定（batch_a/J3 同口径）。
bool j14_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 主干共体：{0}%令对手{1}（申报制，916/J3 同口径）；返回是否落地 ──
bool j14_try_main(BattleContext* ctx, int owner, int target, int chance, int anomaly) {
    if (!g_core || !g_core->apply_anomaly_checked || chance <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return false;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    return j14_landed(r);
}

// 对手处于能力下降状态（任一负槽；六槽含命中，J13 同判据）。
bool has_any_stat_drop(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1024 兜底 tick —— 回合结束一次性：掷歪回合的 {2}%×实际受伤量 FIXED 粉伤 ──
// payload = {owner, opp, pct, hp_at_roll}；受伤量 = hp_at_roll - 当前 hp。
EffectResult j14_1024_fallback_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    const int hp_at_roll = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int taken = hp_at_roll - ctx->getPet(owner).hp;
    if (taken <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, taken * pct / 100, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 1024 每回合掷点（850 roll_and_grant 同款）：掷中挂当回合免攻票，
//    掷歪登记当回合 ROUND_END 一次性兜底 ──
void j14_1024_roll_round(BattleContext* ctx, int owner, int opp, int chance, int pct) {
    if (!ctx || owner < 0 || owner > 1 || opp < 0 || opp > 1 || chance <= 0 || pct <= 0) {
        return;
    }
    if (roll_percent(chance)) {
        ctx->rule_center_.grant_immune(
            owner, static_cast<int>(ImmunityType::DAMAGE), ~0ULL, /*anomaly_mask=*/0,
            /*duration_rounds=*/1, ctx->roundCount, /*source_id=*/0, /*soul=*/false,
            EffectScope::ON_STAGE, /*source_slot=*/-1, /*counts=*/0,
            /*source_effect_id=*/1024);
        return;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, kJ14Node1024Fallback,
                          /*rounds=*/1, {owner, opp, pct, ctx->getPet(owner).hp},
                          &j14_1024_fallback_tick);
}

// ── 1024 第 2..{0} 回合的 ROUND_START tick ──
// payload = {owner, opp, chance, pct}；InRounds 窗口由 register_next_rounds_effect 显式
// 从 R+1 起算（首回合已在用技时掷过，850 首跳同款切分）。
EffectResult j14_1024_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    j14_1024_roll_round(ctx, args.int_args[0], args.int_args[1], args.int_args[2],
                        args.int_args[3]);
    return EffectResult::kOk;
}

}  // namespace

// ════════════════════════ 5 宏 ════════════════════════

// ── 966 —— {0}%的概率使对手{1}，未触发则自身全属性+{2} ──
EffectResult effect_966_chance_or_all_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 966, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 966, args,
                     "966 布局=[owner,target,概率,异常,全属性提升]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int up = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 966, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 966, args,
                     "{1} 须为合法异常码");
    EFFECT_ARG_GUARD(up > 0, 966, args, "需要 全属性提升 > 0");
    if (j14_try_main(ctx, owner, target, args.int_args[2], args.int_args[3])) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, owner, up);   // 六槽含命中；自升走 stat_change 不吃免弱
    return EffectResult::kOk;
}

// ── 1274 —— {0}%令对手{1}，对手处于能力下降状态时触发概率翻倍 ──
EffectResult effect_1274_chance_double_on_drops(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1274, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1274, args,
                     "1274 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1274, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1274, args,
                     "{1} 须为合法异常码");
    EFFECT_ARG_GUARD(chance > 0, 1274, args, "需要 概率 > 0");
    // 翻倍门槛在掷点前求值（对手任一负槽）；封顶 100。
    const int eff = has_any_stat_drop(ctx, target) ? std::min(2 * chance, 100) : chance;
    ctx->plugin_storage[kAudit1274Base + owner] = eff;   // 审计槽（F9 782 口径）
    (void)j14_try_main(ctx, owner, target, eff, args.int_args[3]);
    return EffectResult::kOk;
}

// ── 1248 —— 对手处于异常状态时{0}%令对手{1} ──
EffectResult effect_1248_anomaly_gate_chance(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1248, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1248, args,
                     "1248 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1248, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1248, args,
                     "{1} 须为合法异常码");
    if (!ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 门槛：对手处于任一异常（2009/1048 同判据）
    }
    (void)j14_try_main(ctx, owner, target, args.int_args[2], args.int_args[3]);
    return EffectResult::kOk;
}

// ── 1488 —— 遇到天敌时{0}%令对手{1} ──
EffectResult effect_1488_predator_gate_chance(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1488, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1488, args,
                     "1488 布局=[owner,target,概率,异常]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1488, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[3]), 1488, args,
                     "{1} 须为合法异常码");
    if (!is_predator(ctx, owner)) {
        return EffectResult::kOk;   // 门槛：对手固有属性克制自身固有属性（common.h）
    }
    (void)j14_try_main(ctx, owner, target, args.int_args[2], args.int_args[3]);
    return EffectResult::kOk;
}

// ── 1024 —— {0}回合内{1}%的概率免疫对手攻击伤害，
//             未触发则回合结束时附加{2}%伤害量的固定伤害 ──
EffectResult effect_1024_chance_immune_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1024, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1024, args,
                     "1024 布局=[owner,target,回合,概率,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1, 1024, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && pct > 0, 1024, args,
                     "需要 回合/概率/pct > 0");
    // 首回合立即掷（850 首跳先例：ROUND_START 桶本回合已跑过）。
    j14_1024_roll_round(ctx, owner, opp, chance, pct);
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1024, rounds - 1,
                                    {owner, opp, chance, pct}, &j14_1024_round_tick);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(966, effect_966_chance_or_all_up)
SKILL_EFFECT(1274, effect_1274_chance_double_on_drops)
SKILL_EFFECT(1248, effect_1248_anomaly_gate_chance)
SKILL_EFFECT(1488, effect_1488_predator_gate_chance)
SKILL_EFFECT(1024, effect_1024_chance_immune_window)
