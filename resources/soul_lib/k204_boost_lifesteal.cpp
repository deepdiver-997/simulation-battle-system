// K204 强化吸血族 —— effect 312（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 312，2026-10-07）：
//   312：2626 混沌大天使 [1,100,30]「自身处于能力提升状态时，造成伤害的30%恢复自身体力」
//       → db[0]/db[1] 语义存疑不消费、db[2]=30 比例。
// 实现：EVENT_TAKE_DAMAGE watcher（K200 直系，投递在 AFTER_ACTION drain）：
//       ev.target==对手 && ev.actor==own && ev.amount>0 && 自身任一能力等级>0
//       （K165 门同构取反）→ heal_amount(own, amount×db[2]/100)（吃封回血）。
// ⚠️ 三查记录（2026-10-07）：①内核无 312 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990718000;   // +宏*10+owner：watcher 已挂旗标

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

bool has_any_positive_level(BattleContext* ctx, int owner) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── effect 312：能力提升时吸血 30% ─────────────────────────────────────────

EffectResult k204_312_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 312, owner)) {
        return EffectResult::kOk;
    }
    const int pct = [&] {
        const int slot = ctx->find_pet_with_soulmark(owner, 312);
        if (slot < 0 || slot > 5) {
            return 0;
        }
        const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        return db.size() > 2 ? db[2] : 0;
    }();
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.actor != owner || ev.target == owner || ev.amount <= 0) {
                return;   // 只认自己打出去的实际伤害
            }
            if (!onstage_has_soul_mark(wctx, owner, 312)
                || !has_any_positive_level(wctx, owner)) {
                return;   // 「自身处于能力提升状态时」门
            }
            (void)g_core->heal_amount(wctx, owner, ev.amount * pct / 100);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(312, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k204_312_arm,
                    /*once=*/false, /*early=*/true)});
