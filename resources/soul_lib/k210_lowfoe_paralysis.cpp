// K210 低血攻附麻痹族 —— effect 296（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 296，2026-10-07）：
//   296：2585 净天云神 [1,50,6]「对手体力少于1/2时，每次攻击对手6%的概率麻痹」
//       （args 语义存疑不消费记档）
// 实现：EVENT_SKILL_USED watcher（K176/358 同窗：own + used_attribute 属性技排除）→
// 对手 hp < max/2 门（投递时点现值，工单口径）→ 申报通道 apply_anomaly_checked
// (麻痹0, 6%, SoulMark)（码 0 已验；不预掷）。
// ⚠️ 三查记录（2026-10-07）：①内核无 296 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990719000;   // +宏*10+owner：watcher 已挂旗标

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 296：对手低血时攻击概率麻痹 ─────────────────────────────────────

EffectResult k210_296_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 296, owner)) {
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
                return;   // 「每次攻击」：攻击技
            }
            if (!onstage_has_soul_mark(wctx, owner, 296)) {
                return;
            }
            const ElfPet& foe = wctx->getPet(wctx->opponent(owner));
            const int foe_max = resolve_pet_max_hp(foe);
            if (foe_max <= 0 || foe.hp * 2 >= foe_max) {
                return;   // 「对手体力少于1/2」门
            }
            (void)g_core->apply_anomaly_checked(
                wctx, wctx->opponent(owner), static_cast<int>(AbnormalStatusId::Paralysis),
                random_anomaly_duration(), owner, /*chance_pct=*/6, ChanceSource::SoulMark);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(296, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k210_296_arm,
                    /*once=*/false, /*early=*/true)});
