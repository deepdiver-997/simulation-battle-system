// K198 未受击回血族 —— effect 281（1 宏 1 行，零参）
//
// 行清单（tools/soul_effects_dump.py 281，2026-10-07）：
//   281：2518 潘多拉「若回合内没有受到伤害，则恢复100点体力」（零参）
// 实现：双节点——
//   ① early arm 挂 EVENT_TAKE_DAMAGE watcher（1067 同门 arm-once 旗标）：
//      ev.target==owner && amount>0 → 置受击旗（值=roundCount）。
//      事件面覆盖：TAKE_DAMAGE 在统一扣血路径发射（红/粉固/粉%/真伤全走，含护盾吸收
//      后实际扣血——battle_primitives.cpp:988 注释锚点），amount=实际扣血。
//   ② BATTLE_OLD_ROUND_END_1 节点：受击旗≠本回合 → heal_amount(100)，清旗。
// ⚠️ 边界记档：回合内受击旗按 roundCount 打值——同回合多段受伤只置一次；下场/换人
//    不清旗由 onstage 门兜住（未受击才回血，旗残留下回合依旧只认本回合受击）。
// ⚠️ 三查记录（2026-10-07）：①内核无 281 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990715000;   // +宏*10+owner：watcher 已挂旗标
constexpr int kHitKeyBase = 990716000;     // +owner：受击旗（值=受击回合数）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 281：回合内未受伤 → 战阶结束回 100 ──────────────────────────────

EffectResult k198_281_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 281, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.amount <= 0) {
                return;   // 只认"自己实际掉了血"
            }
            wctx->plugin_storage[kHitKeyBase + owner] = wctx->roundCount;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult k198_281_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 281)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kHitKeyBase + owner);
    if (it != ctx->plugin_storage.end()
        && std::any_cast<int>(it->second) == ctx->roundCount) {
        return EffectResult::kOk;   // 本回合受过伤：不回血
    }
    (void)g_core->heal_amount(ctx, owner, 100);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双节点）────────
SOUL_PROGRAM(281, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k198_281_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k198_281_round_end)});
