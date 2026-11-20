// K189 伤害阈值双分支族 —— effect 1016（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 1016，2026-10-07）：
//   1016：3418 大乔 两行 [280,80,280,1,1]/[280,100,280,1,1]（同构互锚，row2 100% 档）
//     「自身攻击伤害>{db[0]}则{db[1]}%令对手冰封（boss无效）；<{db[0]}则自身下回合
//      所有技能先制+1且攻击必爆」→ db[2]=阈值回声（不消费）、db[3]/db[4]=先制/必爆旗恒1。
// 实现（双节点）：
//   ① 攻击方 DETECT 票（293 同位）：d.final 分叉——
//      final > db[0] → roll_percent(db[1]) → apply_anomaly_checked(对手, 冰封15)
//      （冰封已建模：衍化 冰封→冻伤+速-1 battleFsm.cpp:2145、附带弱化 :592；
//      100% 申报档短路不耗 rand）；
//      final < db[0] → plugin_storage 置下回合旗（db[3]）。
//   ② BATTLE_FIRST_MOVE_RIGHT 节点（chaos_mozun_1263 同款消费形）：旗在 →
//      ws.preemptive_level[owner]+1（db[3]；「所有技能」无攻击门）+
//      ws.must_crit[owner]=true（db[4]；暴击判定只对攻击技跑）→ 清旗。
// ⚠️ 三查记录（2026-10-07）：①内核无 1016 硬编码；②soul_lib 并集无注册；
// ③两行 5 参同构互锚；4399✓3418。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990706000;   // +宏*10+owner：管线已接线旗标（防 early 双触发）
constexpr int kPendingKeyBase = 990707000; // +owner：下回合先制/必爆待消费旗

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── ① DETECT 分叉：高伤→概率冰封；低伤→置下回合旗 ──────────────────────────

EffectResult k189_1016_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 1016, owner)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, 1016);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int threshold = db.size() > 0 ? db[0] : 0;
    const int ice_pct = db.size() > 1 ? db[1] : 0;
    const int preempt_flag = db.size() > 3 ? db[3] : 0;
    if (threshold <= 0 || (ice_pct <= 0 && preempt_flag <= 0)) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, threshold, ice_pct, preempt_flag](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner || !g_core) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤分叉
            }
            if (!onstage_has_soul_mark(c, owner, 1016)) {
                return;
            }
            if (d.final > threshold) {
                // 概率申报通道不预掷（K191 勘误同源：预掷+申报=双重掷点，80% 档塌 64%）
                if (ice_pct > 0 && g_core->apply_anomaly_checked) {
                    (void)g_core->apply_anomaly_checked(
                        c, c->opponent(owner), static_cast<int>(AbnormalStatusId::Icebound),
                        random_anomaly_duration(), owner, ice_pct, ChanceSource::SoulMark);
                }
            } else if (d.final < threshold && preempt_flag > 0) {
                c->plugin_storage[kPendingKeyBase + owner] = 1;   // 下回合先制/必爆待消费
            }
        });
    return EffectResult::kOk;
}

// ── ② 下回合 FMR：先制+1（所有技能）+ 必爆 ─────────────────────────────────

EffectResult k189_1016_fmr_consume(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1016)) {
        return EffectResult::kOk;
    }
    const int key = kPendingKeyBase + owner;
    if (!ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(key);   // 一次性消费
    const int slot = ctx->find_pet_with_soulmark(owner, 1016);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 3 && db[3] > 0) {
        ctx->ws.preemptive_level[owner] += 1;   // 「所有技能先制+1」（无攻击门）
    }
    if (db.size() > 4 && db[4] > 0) {
        ctx->ws.must_crit[owner] = true;   // 必爆（暴击判定只对攻击技跑）
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双节点）────────
SOUL_PROGRAM(1016, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k189_1016_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &k189_1016_fmr_consume)});
