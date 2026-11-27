// K192 连用增伤+战阶条件分支族 —— effect 1310（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 1310，2026-10-07）：
//   1310：3735 苍天歌 [20,80,80,1,1]「自身每使用1次攻击技能则本次战斗中造成的伤害提升20%，
//       最高80%；战斗阶段结束时，若自身体力低于对手则80%的概率使对手害怕，未触发则下回合
//       攻击先制+1且必定打出致命一击（boss无效）」/ 4719 巳璟 [40,160,0,0,1]（同族：
//       「…提升40%，最高160%；战斗阶段结束时，若自身体力低于对手则下回合攻击必定打出
//       致命一击」）
//     → db 映射对 4399 行文互锚解出：db[0]=增伤步、db[1]=增伤上限、db[2]=害怕%（0=无
//       害怕子句）、db[3]=先制量（0=无）、db[4]=必爆旗（巳璟 0,0,1=无害怕无先制仅必爆✓）。
// 实现（四节点）：
//   ① EVENT_SKILL_USED（own+!attr）→ 连用计数++（plugin_storage，本次战斗不清）+
//      审计槽写当前申报增幅（990710000+owner，场景无随机断言，F9 782 先例）；
//   ② AMP 零状态票：d.final ×(100+min(计数×db[0], db[1]))/100 实时读（K12 319 同款）；
//   ③ BATTLE_OLD_ROUND_END_1：自身体力<对手 → db[2]>0 则申报通道 roll 害怕(6)打对手
//      （apply_anomaly_checked，不预掷——K191 双掷勘误）；roll 未中或 db[2]=0 →
//      置 FMR 待消费旗；
//   ④ BATTLE_FIRST_MOVE_RIGHT：旗在 → preemptive_level += db[3] + must_crit(db[4])，
//      清旗（chaos_mozun 消费形）。
// ⚠️ 三查记录（2026-10-07）：①内核无 1310 硬编码；②soul_lib 并集无注册；
// ③两行 5 参同构互锚；4399✓3735/4719。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990711000;    // +宏*10+owner：接线旗标（防 early 双触发）
constexpr int kCountKeyBase = 990712000;    // +owner：攻击技连用计数（本次战斗）
constexpr int kAuditKeyBase = 990713000;    // +owner：当前申报增幅%（审计槽，F9）
constexpr int kPendingKeyBase = 990714000;  // +owner：下回合先制/必爆待消费旗

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

struct Row {
    int step = 0;
    int cap = 0;
    int fear_pct = 0;
    int preempt = 0;
    int crit_flag = 0;
    bool valid = false;
};

Row host_row(BattleContext* ctx, int owner, int mark_id) {
    Row r;
    const int slot = ctx->find_pet_with_soulmark(owner, mark_id);
    if (slot < 0 || slot > 5) {
        return r;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() < 5) {
        return r;
    }
    r.step = db[0];
    r.cap = db[1];
    r.fear_pct = db[2];
    r.preempt = db[3];
    r.crit_flag = db[4];
    r.valid = true;
    return r;
}

int attack_count(BattleContext* ctx, int owner) {
    const auto it = ctx->plugin_storage.find(kCountKeyBase + owner);
    return it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
}

// ── ① 连用计数 + 审计槽 ────────────────────────────────────────────────────

EffectResult k192_1310_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 1310, owner)) {
        return EffectResult::kOk;
    }
    const Row row = host_row(ctx, owner, 1310);
    if (!row.valid || row.step <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 1310)) {
                return;
            }
            const Row r = host_row(wctx, owner, 1310);
            const int count = attack_count(wctx, owner) + 1;
            wctx->plugin_storage[kCountKeyBase + owner] = count;
            wctx->plugin_storage[kAuditKeyBase + owner] =
                std::min(count * r.step, r.cap);   // F9 审计槽（无随机断言面）
        },
        WatcherScope::TEAM);
    // ── ② AMP 零状态票（实时读计数）────────────────────────────────────────
    ctx->register_default_damage_effect(
        DamagePhase::AMP, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 1310)) {
                return;
            }
            const Row r = host_row(c, owner, 1310);
            const int boost = std::min(attack_count(c, owner) * r.step, r.cap);
            if (boost > 0) {
                d.final = d.final * (100 + boost) / 100;
            }
        });
    return EffectResult::kOk;
}

// ── ③ 战阶结束：低体门 → 害怕 roll / 置下回合旗 ────────────────────────────

EffectResult k192_1310_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1310)) {
        return EffectResult::kOk;
    }
    const Row row = host_row(ctx, owner, 1310);
    if (ctx->getPet(owner).hp >= ctx->getPet(ctx->opponent(owner)).hp) {
        return EffectResult::kOk;   // 「若自身体力低于对手」门不满足
    }
    const bool fear_fired = row.fear_pct > 0 && g_core && g_core->apply_anomaly_checked
        && [&] {
            // 分支语义需要同步结果：先过闸门改写取有效概率、自掷定分支；
            // 落地按「已掷」申报（-1）——不预掷+申报 = 双重掷点（K191 勘误同源）
            const int effective = ctx->rewrite_anomaly_chance(
                ctx->opponent(owner), owner, row.fear_pct, ChanceSource::SoulMark);
            return roll_soul_percent(*ctx, 1310, 0, owner, ctx->opponent(owner), effective);
        }();
    if (fear_fired) {
        (void)g_core->apply_anomaly_checked(ctx, ctx->opponent(owner),
                                            static_cast<int>(AbnormalStatusId::Fear),
                                            random_anomaly_duration(), owner,
                                            /*chance_pct=*/-1, ChanceSource::SoulMark);
        return EffectResult::kOk;
    }
    if (row.preempt > 0 || row.crit_flag > 0) {
        ctx->plugin_storage[kPendingKeyBase + owner] = 1;
    }
    return EffectResult::kOk;
}

// ── ④ 下回合 FMR：先制 + 必爆 ─────────────────────────────────────────────

EffectResult k192_1310_fmr_consume(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1310)) {
        return EffectResult::kOk;
    }
    const int key = kPendingKeyBase + owner;
    if (!ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(key);
    const Row row = host_row(ctx, owner, 1310);
    if (row.preempt > 0) {
        ctx->ws.preemptive_level[owner] += row.preempt;
    }
    if (row.crit_flag > 0) {
        ctx->ws.must_crit[owner] = true;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，四节点）────────
SOUL_PROGRAM(1310, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k192_1310_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k192_1310_round_end),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &k192_1310_fmr_consume)});
