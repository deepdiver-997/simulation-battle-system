// K215 登场窗受击消强族 —— effect 276（1 宏 1 行，零参）
//
// 行清单（tools/soul_effects_dump.py 276，2026-10-07）：
//   276：2499 格莱奥丹「登场3回合内受到攻击则对手能力增强效果消失」（零参）
// 实现：双节点——
//   ① ENTER 记账节点（每次登场重记，不复用 watcher 的一次性旗）：plugin_storage
//      [kEnterKeyBase+owner] = roundCount（「登场3回合内」以持有方在场起算）；
//   ② early arm 挂 EVENT_TAKE_DAMAGE watcher（arm-once 旗标）：ev.target==own &&
//      ev.state∈攻击伤害时点（K2 262「受到攻击」同门）&& amount>0 →
//      roundCount - 登场回合 < 3（含登场回合共 3 回合窗）→
//      clear_stat_boosts(ev.actor)（受击方清攻方正等级）。
// ⚠️ 三查记录（2026-10-07）：①内核无 276 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990721000;   // +宏*10+owner：watcher 已挂旗标
constexpr int kEnterKeyBase = 990722000;   // +owner：登场回合账本

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 276：登场 3 回合内受击 → 消攻方强化 ─────────────────────────────

EffectResult k215_276_enter_record(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 276)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kEnterKeyBase + owner] = ctx->roundCount;   // 每次登场重记
    return EffectResult::kOk;
}

EffectResult k215_276_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 276, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->clear_stat_boosts) {
                return;
            }
            if (ev.target != owner || ev.amount <= 0) {
                return;
            }
            // 「受到攻击」：只认攻击伤害时点（K2 262 同门）
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            const auto it = wctx->plugin_storage.find(kEnterKeyBase + owner);
            if (it == wctx->plugin_storage.end()) {
                return;
            }
            if (wctx->roundCount - std::any_cast<int>(it->second) >= 3) {
                return;   // 「登场3回合内」窗外
            }
            (void)g_core->clear_stat_boosts(wctx, ev.actor);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双节点）────────
SOUL_PROGRAM(276, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k215_276_enter_record),
                   SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k215_276_arm,
                             /*once=*/false, /*early=*/true)});
