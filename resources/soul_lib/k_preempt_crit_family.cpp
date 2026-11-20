// K166 命中降/先制减伤/暴击伤族 —— effect 284/282/283（3 宏 6 行）
//
// 行清单（tools/soul_effects_dump.py 284 282 283，2026-10-05）：
//   284 2 行（2522 诸葛亮 / 2523 卧龙诸葛，args空）：「登场时使对手命中-1」
//       → ENTER early stat_drop(命中槽5, 1)（查免弱原语）；2523 第二子句「背包中
//       桃 buff 联动恢复」记档（需刘关张专属 buff 机制）
//   282 2 行（2516 影战·萨格罗斯 args[pct,0]）：「受到先制技能的伤害减少20%/30%」
//       → REDUCE_PCT 票，门 = ws.preemptive_level[attacker] > 0（攻方 MOVE_RIGHT
//       已写、伤害时点可读；含技能固有 priority——正是"先制技能"语义）
//   283 2 行（2524 幻天·萨格罗斯）：「致命一击造成的伤害提升10%/20%」
//       → AMP 票 + 门 = DamageSnapshot.isCrit（暴击判定后可读，query_usage ①.5）。
//       ⚠️ 第二行 tips"+20%" vs args 15 冲突——按立单裁决走 db[0]（头注留痕，白班
//       可对 4399 载体文复核）
//
// 三查记录（2026-10-05，立单时已做）：内核零硬编码、effect_icon kind 已核、registry
// 无三 id。本单为并行会话立单（K 板散点批次），night-author-4 认领施工。
#include "common.h"

namespace {

// arm 幂等守卫（int 槽版，先 find 后 insert——K5 事故口径）。
bool armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(storage_key);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(storage_key, int{-1}).first;
    }
    int* armed_epoch = std::any_cast<int>(&it->second);
    if (!armed_epoch) {
        return true;
    }
    if (*armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    *armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── 284：登场使对手命中 -1 ─────────────────────────────────────────────────

EffectResult soulmark_284_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 284)) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 284 * 100 + 1)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->stat_drop) {
        g_core->stat_drop(ctx, ctx->opponent(owner), /*命中槽=*/5, 1, owner);
    }
    return EffectResult::kOk;
}

// ── 282：受到先制技能伤害 -N% ─────────────────────────────────────────────

EffectResult soulmark_282_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 282 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 282);
    const int pct = db_at(row, 0, row.monster_id == 2516 ? 20 : -1);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 282)) {
                return;
            }
            // "先制技能" = 攻方先制等级 >0（含技能固有 priority，MOVE_RIGHT 已写）。
            if (d.attackerId < 0 || c->ws.preemptive_level[d.attackerId] <= 0) {
                return;
            }
            d.final = d.final * (100 - pct) / 100;
        });
    return EffectResult::kOk;
}

// ── 283：致命一击伤害 +N%（isCrit 门 AMP）─────────────────────────────────

EffectResult soulmark_283_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 283 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 283);
    const int pct = db_at(row, 0, row.monster_id == 2524 ? 10 : -1);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::AMP, owner, DamageEffectCategory::AMP,
        [owner, pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!d.isCrit) {
                return;   // 只放大致命一击
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 283)) {
                return;
            }
            d.addPct += pct;
            d.final = d.final * (100 + pct) / 100;
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（只增不改）────────────────
SOUL_PROGRAM(284, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_284_enter,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(282, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_282_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(283, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_283_arm,
                   /*once=*/false, /*early=*/true)});
