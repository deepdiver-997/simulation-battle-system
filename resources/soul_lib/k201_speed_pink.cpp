// K201 速度提升回合末固伤族 —— effect 334（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 334，2026-10-07）：
//   334：2676 炽凰·朱雀「速度处于提升状态时，回合结束减少对手1/6最大HP」
//       （args[5,6] 语义存疑，不消费记档）
// 实现：BATTLE_OLD_ROUND_END_1 节点（yanshou 形态）+ onstage 门 →
// ability_levels[owner][速度槽4] > 0 → deal_pink_damage(对手, max/6, PERCENT_VALUE)
// （encarlos 3577:173 同款 1/N 最大 HP 粉伤；1/6 整除取整记档；粉伤吃抗性/护罩/免粉）。
// ⚠️ 三查记录（2026-10-07）：①内核无 334 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kSpeedSlot = 4;   // 等级槽序 0攻 1特攻 2防 3特防 4速 5命中

// ── effect 334：速度提升 → 回合结束削对手 1/6 最大 HP ──────────────────────

EffectResult k201_334_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 334)) {
        return EffectResult::kOk;
    }
    if (ctx->ability_levels[owner][kSpeedSlot] <= 0) {
        return EffectResult::kOk;   // 速度未处于提升状态
    }
    const int opp = ctx->opponent(owner);
    const int foe_max = resolve_pet_max_hp(ctx->getPet(opp));
    if (foe_max <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, foe_max / 6, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(334, {SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k201_334_round_end)});
