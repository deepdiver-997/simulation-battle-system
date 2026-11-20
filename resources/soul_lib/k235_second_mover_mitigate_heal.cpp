// K235 · 后出手减伤+回血族 —— effect 705（1 宏 1 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k235）
//
// 行清单（tools/soul_effects_dump.py 705，2026-10-08）：
//   705 row378（真龙·塔西亚 3004）args[50,200]（kind[3,5]，单行）
//     「后出手时受到的伤害减少50%,后出手时恢复200点HP」
//   → db[0]=减伤%；db[1]=回合末回血值。tips 与 args 自洽（SOP §六.1）。
//
// 实现口径：
//   - "后出手"判定 = ctx->is_second_mover(owner)（battleContext.h 引擎 helper：
//     先手权由 MOVE_RIGHT 时点结算写入 ws.preemptive_right）。⚠️ 实测记档：FMR 桶
//     执行时点 ws.preemptive_right 尚未落值（handle 入口清零、桶后结算）——桶内读
//     恒 NONE（首跑 ledger=0 事故），改**消费时点直读**：
//     减伤票 = REDUCE_PCT + MITIGATE 回调内直读（伤害时点先手权早已结算）；
//     回血 = BATTLE_ROUND_END 节点直读（is_second_mover 1 → heal_amount(db[1])，
//     K226 形；原语吃封回血/修正/满血封顶）；
//   - 减伤只红伤（"受到的伤害"按 K184/K229 家族口径红伤票，粉伤不沾记档）；
//   - （BOSS无效）：SoulMarkNodeRef boss_invalid=true + K190 口径（记档）。
//
// 动工前置三查（2026-10-08，author k-prod-6）：①内核 grep 705 无魂印行为硬编码；
//   ②effect_icon kind=[3,5]、tips 为权威文（dump 原文已核）；③registry 无 705、
//   soul_lib 并集（hub+feat/k226-k234 全分支 grep SOUL_PROGRAM）无 705——真缺口。
// 先例：K227 2011（BATTLE_FIRST_MOVE_RIGHT 节点形 + boss_invalid 参数）、K184/K229
//   （REDUCE_PCT 票形）、K226（ROUND_END heal）。
// 场景回归：test/scenario/scenario_3790_second_mover_mitigate_heal_705.cpp（号段 3790-3799）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 705;
constexpr int kWiredKeyBase = 991436000;    // +owner：减伤票已挂旗标

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K226 同款私副本）──────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
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
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── early：挂一次减伤票（后手判定在伤害时点直读）────────────────────────────

EffectResult soulmark_705_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int reduce_pct = db_at(row, 0, -1);
    if (reduce_pct < 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, reduce_pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMark)) {
                return;
            }
            if (!c->is_second_mover(bucket_owner)) {
                return;   // 先出手 → 无减伤（伤害时点先手权已结算）
            }
            d.final = d.final * (100 - reduce_pct) / 100;
        });
    return EffectResult::kOk;
}

// ── ROUND_END：后出手 → 回 db[1] HP ────────────────────────────────────────

EffectResult soulmark_705_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int heal = db_at(row, 1, -1);
    if (heal > 0 && g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：705）。early=减伤票 arm-once；ROUND_END=回血（后手判定两处均为
// 消费时点直读，见首跑记档）。
SOUL_PROGRAM(705, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_705_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_705_round_end,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});
