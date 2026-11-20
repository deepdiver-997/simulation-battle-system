// E 组批产 · E5 暴击触发族（2026-09-29；工单：docs_local/docs/05-任务清单/E组批量效果工单.md）
//
//   2 效果一个家族模板：
//     496 若打出致命一击则恢复自身全部体力      （切片 []，如 战神狂龙破 14427）
//     553 若打出致命一击则对手{0}%概率{1}       （切片 [prob,anomaly]，如 半月斩 [100,6]=100% 害怕）
//   动工三查（工单协议②）：内核 switch 表 / effect_meta 无硬编码、moves_lib 无注册——真缺口。
//
//   判据（batch_a 731 先例，2026-09-28 夜跑已并）：crit_happened 在 query_usage ①.5
//   （ON_SKILL_HIT）掷、SKILL_EFFECT 时已可见；miss/打盔不进 SKILL_EFFECT →
//   "打出"语义天然成立——直接在 SKILL_EFFECT 读旗标，无需 AFTER_ACTION 桥
//   （工单"模板先例"一栏预判的桥在此省略，731 已定口径）。
//   553 概率走 apply_anomaly_checked 申报（亮节闸门管辖，ChanceSource::Skill；
//   duration 随机异常时长 random_anomaly_duration()）；496 满血 =
//   heal_amount(max_hp_of)（原语吃封回血/修正%，内部钳 max——batch_a 同件）。

#include "common.h"

namespace {

// ── 496 —— 若打出致命一击则恢复自身全部体力。Args: [owner, 占位]。─────────────
EffectResult effect_496_crit_full_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 496, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 2, 496, args, "需要 [owner,占位]");
    EFFECT_ARG_GUARD(g_core && g_core->heal_amount != nullptr, 496, args,
                     "需要 heal_amount 原语");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 496, args, "owner 越界");
    if (!ctx->crit_happened[owner]) {
        return EffectResult::kOk;   // 没打出致命一击：不回血
    }
    (void)g_core->heal_amount(ctx, owner, max_hp_of(ctx, owner));
    return EffectResult::kOk;
}

// ── 553 —— 若打出致命一击则对手{0}%概率{1}。Args: [owner, 占位, prob, anomaly]。──
EffectResult effect_553_crit_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 553, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 553, args, "需要 [owner,占位,prob,anomaly]");
    EFFECT_ARG_GUARD(g_core && g_core->apply_anomaly_checked != nullptr, 553, args,
                     "需要 apply_anomaly_checked 原语");
    const int owner = args.int_args[0];
    const int prob = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 553, args, "owner 越界");
    if (!ctx->crit_happened[owner]) {
        return EffectResult::kOk;   // 没打出致命一击：不施加
    }
    (void)g_core->apply_anomaly_checked(ctx, 1 - owner, anomaly, random_anomaly_duration(),
                                        owner, prob, ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(496, effect_496_crit_full_heal);
SKILL_EFFECT(553, effect_553_crit_anomaly);
