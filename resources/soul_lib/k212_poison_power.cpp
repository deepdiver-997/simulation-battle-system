// K212 对手中毒威力翻倍族 —— effect 308（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 308，2026-10-07）：
//   308 魔灵仙女 2798 args[2, 1, 100]：「若对手中毒则攻击技能威力翻倍」
//     → args 语义存疑不消费（记档）；翻倍由 tips 字面承载。
//
// ⚠️ 实现形态（T4 类型化行参）：SKILL_EFFECT 时点 view 增伤（K165 291 直系同门，
//    FIRST/SECOND 双桶）：roundChoice 门 + 类型表门 → has_active_anomaly(opp, 中毒1)
//    门 → skill_power_view[owner] ×2。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 308 魂印行为硬编码 ②effect_info
//    无 308 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k165_gate_attach_power（291 SKILL_EFFECT view 全形态——同语义异 id）、K187。
// 场景回归：test/scenario/scenario_3540_poison_power.cpp（号段 3540-3549）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 308;
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);   // 1

EffectResult k212_308_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5
        || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技（K4 判据：类型表门）
    }
    if (!g_core->has_active_anomaly(ctx, ctx->opponent(owner), kPoison)) {
        return EffectResult::kOk;   // 门：对手处于中毒
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（308 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT,
                             &k212_308_skill_effect),
                       SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT,
                                 &k212_308_skill_effect)});
