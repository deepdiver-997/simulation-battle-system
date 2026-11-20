// K203 暴击概率害怕族 —— effect 344（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 344，2026-10-07）：
//   344：2709 太古邪神「若打出致命一击则对手30%概率害怕」（args[30,6]=概率/害怕码，
//       与行文一致互锚）
// 实现：攻击方 DETECT 票（1880/393 同窗）+ d.isCrit 门——暴击在 query_usage ①.5 判定
// （SOP §五.4），DETECT 相位（管线内）已可读（内核先例 1323「读结算后的
// resolvedDamage.isCrit」，skills.cpp:186）→ 申报通道 apply_anomaly_checked
// (害怕6, 30%, SoulMark)（不预掷，K191 双掷勘误）。
// ⚠️ 三查记录（2026-10-07）：①内核无 344 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990717000;   // +宏*10+owner：管线已接线旗标

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── effect 344：打出致命一击 → 对手 30% 害怕 ───────────────────────────────

EffectResult k203_344_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 344, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0 || !d.isCrit) {
                return;   // 只认自己的暴击红伤
            }
            if (!onstage_has_soul_mark(c, owner, 344)) {
                return;
            }
            (void)g_core->apply_anomaly_checked(
                c, c->opponent(owner), static_cast<int>(AbnormalStatusId::Fear),
                random_anomaly_duration(), owner, /*chance_pct=*/30, ChanceSource::SoulMark);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(344, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k203_344_arm,
                    /*once=*/false, /*early=*/true)});
