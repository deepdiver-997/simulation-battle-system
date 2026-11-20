// J 组 J57 · 后手门族二 —— 1357×17 / 1092×16 / 1401×19（2026-10-03，worktree ../agents/j57）
//
// 家族语义（effect_info 官方模板即权威），三条都是"后出手时 → X（未触发则 Y）"：
//   1357（arg_count=1）"后出手时造成的伤害提高{0}%"
//   1092（arg_count=2）"后出手时使对手{0}，未触发则附加自身当前体力1/{1}的百分比伤害"
//   1401（arg_count=3）"后出手时{0}%令对手{1}，未触发则使自身下{2}回合必定致命一击"
//
// 实现口径：
//   - 后手门 = acting_side_is_first 取反（common.h 助手，J49 476 直系——SKILL_EFFECT
//     镜像 FIRST/SECOND 两侧时点，currentState 即出手侧）。
//   - 1357 = ws.damage_add_pct 直写落本次攻击 snapshot（effect_set_damage_amp，
//     920/1439 同通道；"提高{0}%"平直加法，"额外提升"才是 AMP_EXTRA 乘法——1103 口径）。
//   - 1092 = apply_anomaly_checked 概率申报版（1978 直系：亮节闸门管辖、
//     "未触发"= 未落地 anomaly_landed，免疫/抗性/反弹都算）→ fallback
//     deal_pink_damage(自身**当前**体力/{1}，PERCENT_VALUE)（J50 644 当前形）。
//   - 1401 = 同 1092 申报制（{0}% 概率）→ fallback must_crit NextRounds {2}
//     （crawford 1600 未触发必暴同款；tick 写 ws.must_crit，681/1204 同通道）。
//
// 载体切片（unity 库 json_each 精确匹配 17/16/19=52；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1357 [pct]：{50}×8/{100}×6/{60}×2/{200}×1（纯链载体 以静制动 36504 [100] 特130）
//   1092 [异常码,分母]：{6,3}×3/{15,3}×3/{22,3}×2…（纯链载体 轰天双臂转 33493 [6,3] 物150）
//   1401 [概率,异常码,必暴回合]：全 17 组合（纯链载体 魁痕 36430 [100,16,1] 物120）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1357/1092/1401 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：476/969（后手门）、1103/1198（amp 直写落 snapshot）、1978（申报制异常+
// 未触发弹粉）、644（当前体力形）、1600（未触发必暴 NextRounds）。
// 场景回归：test/scenario/scenario_1430_second_gate2.cpp（1430 号段，一文件三宏）。

#include "common.h"

namespace {

// batch_a 同判据（各文件自备一份）："落地"= 成功/替换/续时/转化。
bool j57_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 1401 的必暴 tick —— 未触发 → 下{2}回合每回合必暴（1600/1204 同通道）──
EffectResult j57_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[self] = true;
    return EffectResult::kOk;
}

// ── 1357 —— 后出手时造成的伤害提高{0}% ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_1357_second_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1357, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1357, args,
                     "1357 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1357, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 1357, args, "需要 pct > 0");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门
    }
    // 平直加法 amp，落本次攻击 snapshot（SKILL_EFFECT 时点早于 AMP 结算）。
    EffectArgs tick_args(std::vector<int>{owner, args.int_args[1], pct});
    return effect_set_damage_amp(ctx, tick_args);
}

// ── 1092 —— 后出手时使对手{0}，未触发则附加自身当前体力1/{1}的百分比伤害 ──
// int_args = [owner, target, anomaly, denom]（args_num=2）。
EffectResult effect_1092_second_anomaly_or_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1092, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1092, args,
                     "1092 布局=[owner,target,异常码,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1092, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1092, args, "需要 分母 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1092, args,
                     "需要 异常码 ∈ [0,43]");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 概率申报版（亮节闸门管辖）；未落地（免疫/抗性/反弹）= "未触发"（1978 判据）。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, /*chance=*/100,
        ChanceSource::Skill);
    if (j57_anomaly_landed(r)) {
        return EffectResult::kOk;
    }
    if (g_core->deal_pink_damage) {
        // "自身**当前**体力"1/{1}（644 当前形；模板无概率字样 → 全力争，100 申报）。
        (void)g_core->deal_pink_damage(ctx, opp, ctx->getPet(owner).hp / denom,
                                       DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1401 —— 后出手时{0}%令对手{1}，未触发则使自身下{2}回合必定致命一击 ──
// int_args = [owner, target, chance, anomaly, crit_rounds]（args_num=3）。
EffectResult effect_1401_second_anomaly_or_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1401, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1401, args,
                     "1401 布局=[owner,target,概率,异常码,必暴回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int crit_rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1401, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && crit_rounds > 0, 1401, args, "需要 概率/必暴回合 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1401, args,
                     "需要 异常码 ∈ [0,43]");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance,
        ChanceSource::Skill);
    if (j57_anomaly_landed(r)) {
        return EffectResult::kOk;
    }
    // 未触发 → 下{2}回合必暴（1600 同款；NextRounds 本回合不算）。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1401, crit_rounds,
                                {owner}, &j57_must_crit_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1357, effect_1357_second_amp)
SKILL_EFFECT(1092, effect_1092_second_anomaly_or_pink)
SKILL_EFFECT(1401, effect_1401_second_anomaly_or_crit)
