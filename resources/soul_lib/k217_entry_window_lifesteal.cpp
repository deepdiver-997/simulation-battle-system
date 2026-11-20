// K217 登场窗吸血族 —— effect 278（1 宏 1 行，零参）
//
// 行清单（tools/soul_effects_dump.py 278，2026-10-07）：
//   278：2509 诺伊尔顿「登场3回合内所有攻击造成伤害30%将恢复体力」（零参）
// 实现：双节点——
//   ① ENTER 记账节点（每次登场重记）：plugin_storage[kEnterKey+owner]=roundCount；
//   ② early arm 挂 EVENT_TAKE_DAMAGE watcher（arm-once 旗标，K200/312 直系）：
//      ev.actor==own && ev.target!=own && amount>0 且窗内（roundCount-登场回合<3，
//      含登场回合）→ heal_amount(own, amount×30/100)（吃封回血）。
// ⚠️ 三查记录（2026-10-07）：①内核无 278 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990724000;   // +宏*10+owner：watcher 已挂旗标
constexpr int kEnterKeyBase = 990725000;   // +owner：登场回合账本

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 278：登场 3 回合内攻击吸血 30% ──────────────────────────────────

EffectResult k217_278_enter_record(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 278)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kEnterKeyBase + owner] = ctx->roundCount;   // 每次登场重记
    return EffectResult::kOk;
}

EffectResult k217_278_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 278, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->heal_amount) {
                return;
            }
            if (ev.actor != owner || ev.target == owner || ev.amount <= 0) {
                return;   // 只认自己打出去的实际伤害
            }
            const auto it = wctx->plugin_storage.find(kEnterKeyBase + owner);
            if (it == wctx->plugin_storage.end()
                || wctx->roundCount - std::any_cast<int>(it->second) >= 3) {
                return;   // 「登场3回合内」窗外
            }
            if (!onstage_has_soul_mark(wctx, owner, 278)) {
                return;
            }
            (void)g_core->heal_amount(wctx, owner, ev.amount * 30 / 100);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双节点）────────
SOUL_PROGRAM(278, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k217_278_enter_record),
                   SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k217_278_arm,
                             /*once=*/false, /*early=*/true)});
