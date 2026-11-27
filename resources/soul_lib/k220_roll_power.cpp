// K220 概率威力翻倍族 —— effect 292（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 292，2026-10-07）：
//   292 圣虎湮光 2566 args[]（零参）：「6%的概率使自己技能威力翻倍」
//
// ⚠️ 实现形态（T4 类型化行参）：SKILL_EFFECT 时点 view 增伤（K212/K165 291 直系，
//    FIRST/SECOND 双桶）：roundChoice 门 + 类型表门 → roll_percent(6)（调用方自掷，
//    293 同款）→ skill_power_view[owner] ×2。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 292 魂印行为硬编码 ②effect_info
//    无 292 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k212_poison_power（SKILL_EFFECT view 门翻倍全形态——本单把门换成掷点）。
// 场景回归：test/scenario/scenario_3620_roll_power.cpp（号段 3620-3629）。

#include "common.h"

namespace {

constexpr int kMarkId = 292;

EffectResult k220_292_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
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
    if (!roll_soul_percent(*ctx, kMarkId, 0, owner, owner, 6)) {
        return EffectResult::kOk;   // 6% 掷点（调用方自掷，293 同款）
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（292 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT,
                             &k220_292_skill_effect),
                       SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT,
                                 &k220_292_skill_effect)});
