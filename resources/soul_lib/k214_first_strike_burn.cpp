// K214 先手概率烧伤族 —— effect 318（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 318，2026-10-07）：
//   318：2646 克洛伊 [1,20,2]「先手时20%概率使对手烧伤」（args 语义存疑不消费，
//       20=概率由行文承载）
// 实现：EVENT_SKILL_USED watcher（K176/358 同窗：own + 属性技排除）+ 先手门
// （is_second_mover 取反——投递时点 pr 仍为本回合结算值，K12 310 记档口径）→
// 申报通道 apply_anomaly_checked(烧伤2, 20%, SoulMark)（不预掷）。
// ⚠️ 三查记录（2026-10-07）：①内核无 318 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990720000;   // +宏*10+owner：watcher 已挂旗标

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 318：先手时攻击概率烧伤 ─────────────────────────────────────────

EffectResult k214_318_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 318, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.actor != owner || ev.used_attribute) {
                return;   // 「先手时（攻击）」：攻击技
            }
            if (!onstage_has_soul_mark(wctx, owner, 318)) {
                return;
            }
            if (wctx->is_second_mover(owner)) {
                return;   // 先手门（pr 投递时点仍为本回合结算值）
            }
            (void)g_core->apply_anomaly_checked(
                wctx, wctx->opponent(owner), static_cast<int>(AbnormalStatusId::Burn),
                random_anomaly_duration(), owner, /*chance_pct=*/20, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(318, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k214_318_arm,
                    /*once=*/false, /*early=*/true)});
