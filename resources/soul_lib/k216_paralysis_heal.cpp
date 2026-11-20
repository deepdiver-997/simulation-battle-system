// K216 概率麻痹+伤害回血族 —— effect 628（1 宏 3 行）
//
// 行清单（tools/soul_effects_dump.py 628，2026-10-07）：
//   628：2887 雷神·雷伊 两行 [25,0,1,25]/[35,0,1,25]（进化行 25%→35%）/
//       2906 极天斗皇 [30,0,1]「每次使用技能都有30%令对手麻痹」（无 db[3]→无回血子句）
//     「所有技能有{db[0]}%概率令对手麻痹（boss无效）;造成伤害的{db[3]}%恢复自身体力」
//     → db[0]=麻痹%、db[1]=异常码(0)、db[2]=boss 位、db[3]=回血%（缺行=子句无）。
// 实现（双节点）：
//   ① EVENT_SKILL_USED watcher（own——行文「所有技能」无系别过滤，K2 114 同款）→
//      申报通道 apply_anomaly_checked(码0麻痹)（K176 已验；不预掷）；
//   ② 攻击方 DETECT 票（393/1880 同窗）→ heal_amount(final×db[3]/100)
//      （「造成伤害的N%恢复」= 落地实际伤害读 d.final，K204 312 差账同款断言）。
// ⚠️ 三查记录（2026-10-07）：①内核无 628 硬编码；②并集无注册；③side_effect 628
//    系技能空间同号（J54，双注册 1004 先例）；4399✓2887/2906。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990723000;   // +宏*10+owner：watcher 已挂旗标

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

int row_db(BattleContext* ctx, int owner, int mark_id, int idx, int fallback) {
    const int slot = ctx->find_pet_with_soulmark(owner, mark_id);
    if (slot < 0 || slot > 5) {
        return fallback;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    return static_cast<int>(db.size()) > idx ? db[idx] : fallback;
}

// ── effect 628：所有技能概率麻痹 + 伤害回血（双子句）────────────────────────

EffectResult k216_628_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 628, owner)) {
        return EffectResult::kOk;
    }
    const int paralysis_pct = row_db(ctx, owner, 628, 0, 0);
    const int heal_pct = row_db(ctx, owner, 628, 3, 0);
    if (paralysis_pct <= 0 && heal_pct <= 0) {
        return EffectResult::kOk;
    }
    if (paralysis_pct > 0) {
        // ① 「所有技能」概率麻痹（无系别过滤，K2 114 同款）
        ctx->event_center_.register_watcher(
            EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, paralysis_pct](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || !g_core || !g_core->apply_anomaly_checked) {
                    return;
                }
                if (ev.actor != owner) {
                    return;   // 行文「所有技能」：属性技也算，无 used_attribute 门
                }
                if (!onstage_has_soul_mark(wctx, owner, 628)) {
                    return;
                }
                (void)g_core->apply_anomaly_checked(
                    wctx, wctx->opponent(owner),
                    static_cast<int>(AbnormalStatusId::Paralysis), random_anomaly_duration(),
                    owner, paralysis_pct, ChanceSource::SoulMark);
            },
            WatcherScope::TEAM);
    }
    if (heal_pct > 0) {
        // ② 伤害回血（DETECT 落地实际伤害）
        ctx->register_default_damage_effect(
            DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
            [owner, heal_pct](BattleContext* c, int bucket_owner) {
                if (bucket_owner != owner || !g_core || !g_core->heal_amount) {
                    return;
                }
                const DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (!onstage_has_soul_mark(c, owner, 628)) {
                    return;
                }
                (void)g_core->heal_amount(c, owner, d.final * heal_pct / 100);
            });
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(628, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k216_628_arm,
                    /*once=*/false, /*early=*/true)});
