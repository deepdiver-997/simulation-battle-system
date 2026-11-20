// K191 强化冰封/降态反转固伤族 —— effect 1796（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 1796，2026-10-07）：
//   1796：4203 卞玉儿 两行 [50,300,1]/[60,400,1]（同构互锚）
//     「回合开始时若自身处于能力提升状态则攻击命中后{db[0]}%令对手冰封（boss无效），
//      回合结束时若自身处于能力下降状态则反转自身能力下降状态且附加{db[1]}点固定伤害」
//     → db[2]=boss 旗（不消费记档；固伤方向 4399✓：打对手）。
// 实现（三节点）：
//   ① BATTLE_ROUND_START 声明节点：自身任一能力等级 >0 → 置当回合旗
//      （plugin_storage 值=roundCount，防跨回合残留）；
//   ② 攻击方 DETECT 票：旗为本回合 && 命中红伤 → roll_percent(db[0]) →
//      apply_anomaly_checked(对手, 冰封15)（冰封已建模，K189 查实）；
//   ③ BATTLE_OLD_ROUND_END_1 节点（战阶结束，K186 同款 State）：自身任一等级 <0 →
//      stat_reversal(owner)（反转下降→提升原语，battle_primitives.h:635）+
//      deal_pink_damage(对手, db[1], FIXED)。
// ⚠️ 三查记录（2026-10-07）：①内核无 1796 硬编码；②soul_lib 并集无注册；
// ③两行 3 参同构互锚；4399✓4203。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kWiredKeyBase = 990708000;   // +宏*10+owner：管线已接线旗标（防 early 双触发）
constexpr int kBoostFlagBase = 990709000;  // +owner：本回合强化旗（值=roundCount）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredKeyBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

bool has_any_level(BattleContext* ctx, int owner, bool positive) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[owner][i];
        if (positive ? lv > 0 : lv < 0) {
            return true;
        }
    }
    return false;
}

// ── ① 回合开始：评自身强化 → 置当回合旗 ────────────────────────────────────

EffectResult k191_1796_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1796)) {
        return EffectResult::kOk;
    }
    const int key = kBoostFlagBase + owner;
    if (has_any_level(ctx, owner, /*positive=*/true)) {
        ctx->plugin_storage[key] = ctx->roundCount;   // 值=回合数，防跨回合残留
    } else {
        ctx->plugin_storage.erase(key);
    }
    return EffectResult::kOk;
}

// ── ② 攻击命中后：强化旗 + roll → 冰封对手 ─────────────────────────────────

EffectResult k191_1796_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 1796, owner)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, 1796);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int ice_pct = db.size() > 0 ? db[0] : 0;
    if (ice_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, ice_pct](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 「攻击命中后」
            }
            const auto it = c->plugin_storage.find(kBoostFlagBase + owner);
            if (it == c->plugin_storage.end()
                || std::any_cast<int>(it->second) != c->roundCount) {
                return;   // 回合开始时无强化
            }
            if (!onstage_has_soul_mark(c, owner, 1796)) {
                return;
            }
            // 概率申报通道：不预掷，原语按申报值掷一次（闸门在其前介入，K2 同款；
            // 预掷+申报 = 双重掷点，实测 50% 档塌成 25%）
            (void)g_core->apply_anomaly_checked(
                c, c->opponent(owner), static_cast<int>(AbnormalStatusId::Icebound),
                random_anomaly_duration(), owner, ice_pct, ChanceSource::SoulMark);
        });
    return EffectResult::kOk;
}

// ── ③ 战阶结束：自身有降态 → 反转 + 附加固伤打对手 ─────────────────────────

EffectResult k191_1796_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->stat_reversal || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1796)) {
        return EffectResult::kOk;
    }
    if (!has_any_level(ctx, owner, /*positive=*/false)) {
        return EffectResult::kOk;   // 无降态：不反转不附加
    }
    const int slot = ctx->find_pet_with_soulmark(owner, 1796);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int fixed = db.size() > 1 ? db[1] : 0;
    (void)g_core->stat_reversal(ctx, owner);   // REVERSED/NOTHING 均继续附加（同句连动）
    if (fixed > 0) {
        (void)g_core->deal_pink_damage(ctx, ctx->opponent(owner), fixed, DamageKind::FIXED,
                                       owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，三节点）────────
SOUL_PROGRAM(1796, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k191_1796_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &k191_1796_round_start),
                   SoulMarkNodeRef(State::BATTLE_OLD_ROUND_END_1, &k191_1796_round_end)});
