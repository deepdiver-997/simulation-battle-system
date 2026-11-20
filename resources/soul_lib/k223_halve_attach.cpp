// K223 首回合减半+短窗附伤族 —— effect 264（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 264，2026-10-07）：
//   264 梵天 2447 args[]（零参）：「首回合受到伤害减半，2回合内任何攻击技能都将附带
//     100点固定伤害」
//
// ⚠️ 实现形态（T4 类型化行参）：enter 记登场回合（账本，K218 同款）+ 单 arm 接线双
//    DETECT/REDUCE 票（EpochGuard 防重，ON_STAGE 绑定）：
//   ①REDUCE_PCT+MITIGATE（K184 直系）：受击红伤且 roundCount==登场回合 → final×50/100。
//   ②DETECT 附粉（K197 直系）：own 红伤落地且 roundCount-登场回合 < 2（含当回合的
//     2 回合窗）→ deal_pink_damage(100, FIXED, owner)。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 264 魂印行为硬编码 ②effect_info
//    无 264 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k184_male_mitigate（REDUCE_PCT 票）、k197_hp_pct_attach（DETECT 附粉）。
// 场景回归：test/scenario/scenario_3670_halve_attach.cpp（号段 3670-3679）。

#include "common.h"

namespace {

constexpr int kMarkId = 264;

struct K223Runtime {
    int entry_round = -1;
};

K223Runtime& k223_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K223Runtime{};
    }
    return std::any_cast<K223Runtime&>(storage[kMarkId]);
}

struct EpochGuard {
    int armed_epoch = -1;
};

bool already_armed_this_epoch(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(kMarkId + 1)) {
        pet.soulmark_storage[kMarkId + 1] = EpochGuard{};
    }
    EpochGuard& guard = std::any_cast<EpochGuard&>(pet.soulmark_storage[kMarkId + 1]);
    if (guard.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    guard.armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

EffectResult k223_264_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || already_armed_this_epoch(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K223Runtime& rt = k223_runtime(ctx, owner);
    rt.entry_round = ctx->roundCount;
    // ① 首回合受伤减半。
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !onstage_has_soul_mark(c, bucket_owner, kMarkId)) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (k223_runtime(c, bucket_owner).entry_round != c->roundCount) {
                return;   // 首回合门
            }
            d.final = d.final * 50 / 100;
        });
    // ② 2 回合内攻击附 100 固定。
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            if (bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const K223Runtime& rt = k223_runtime(c, owner);
            if (rt.entry_round < 0 || c->roundCount - rt.entry_round >= 2) {
                return;   // 2 回合窗（含登场回合）
            }
            (void)g_core->deal_pink_damage(c, c->opponent(owner), 100,
                                           DamageKind::FIXED, owner);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（264 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k223_264_arm,
                        /*once=*/false, /*early=*/true)});
