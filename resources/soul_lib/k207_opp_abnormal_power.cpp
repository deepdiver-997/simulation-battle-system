// K207 对手异常威力翻倍族 —— effect 301（1 宏 1 行，零参）
//
// 行清单（tools/soul_effects_dump.py 301，2026-10-07）：
//   301：2598 绝影·天煞「对手处于异常状态时，威力翻倍」（args 语义存疑不消费记档）
// 实现：SKILL_EFFECT 节点（K165 291 直系同门——291 即同语义「对手异常时威力翻倍」
// 不同 id）：FIRST/SECOND 双桶 + roundChoice 门 + 技能表类型门（该时点 skill_use_seq
// 尚未自增，executed_* 不可读）+ has_any_active_abnormal_status(对手) 门 →
// skill_power_view ×2（写点=物化后、伤害公式前）。
// ⚠️ 三查记录（2026-10-07）：①内核无 301 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

// ── effect 301：对手异常时威力翻倍 ─────────────────────────────────────────

EffectResult k207_301_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 301)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技的威力
    }
    if (!ctx->has_any_active_abnormal_status(1 - owner)) {
        return EffectResult::kOk;   // 门：对手处于异常状态
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 2;   // 「威力翻倍」（零参行）
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双桶）──────────
SOUL_PROGRAM(301, {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k207_301_skill_effect),
                   SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k207_301_skill_effect)});
