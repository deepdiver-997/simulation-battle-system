// K219 首登场窗攻附固伤族 —— effect 288（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 288，2026-10-07）：
//   288：2422 邪恶魔神 [3,50]「首次登场3回合内所有直接攻击附加50点固定伤害」
//     → db[0]=窗长（3 回合）、db[1]=固伤量（50）。
// 实现：双节点——
//   ① ENTER 记账节点：「首次」幂等旗（plugin_storage 已有即不重写——切换重入场不重置
//      窗基点）；record roundCount；
//   ② 攻击方 DETECT 票（393/1880 同窗）：红伤落地门（attackerId+isRed+final>0+onstage）
//      且 roundCount - 首登场回合 < db[0] → deal_pink_damage(对手, db[1], FIXED)。
// ⚠️ 三查记录（2026-10-07）：①内核无 288 硬编码；②effect_info 无同号；③并集无注册。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kFirstKeyBase = 990726000;   // +owner：首登场回合（幂等，重入场不重置）

// ── effect 288：首次登场 3 回合内攻击附加固伤 ───────────────────────────────

EffectResult k219_288_enter_record(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 288)) {
        return EffectResult::kOk;
    }
    const int key = kFirstKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 「首次」：幂等旗，重入场不重置
    }
    ctx->plugin_storage[key] = ctx->roundCount;
    return EffectResult::kOk;
}

EffectResult k219_288_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, 288);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    const int window = db.size() > 0 ? db[0] : 3;
    const int attach = db.size() > 1 ? db[1] : 0;
    if (window <= 0 || attach <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, window, attach](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 「所有直接攻击」：红伤落地
            }
            if (!onstage_has_soul_mark(c, owner, 288)) {
                return;
            }
            const auto it = c->plugin_storage.find(kFirstKeyBase + owner);
            if (it == c->plugin_storage.end()
                || c->roundCount - std::any_cast<int>(it->second) >= window) {
                return;   // 首登场窗外
            }
            (void)g_core->deal_pink_damage(c, c->opponent(owner), attach,
                                           DamageKind::FIXED, owner);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改，双节点）────────
SOUL_PROGRAM(288, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k219_288_enter_record),
                   SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k219_288_arm)});
