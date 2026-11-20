// K249 · 面板双防+低伤门免异常族 —— effect 775（1 宏 1 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k249）
//
// 行清单（tools/soul_effects_dump.py 775，2026-10-13）：
//   775 row467（班纳尔克 3116）args[20,300]（kind[7,11]，单行）
//     「提升自身体力20%的双防值；攻击技能造成的伤害低于300则下回合免疫异常状态」
//   → db[0]=双防提升比%（自身体力×20%）；db[1]=低伤门（raw < db[1]）。tips 为权威文。
//
// 实现口径：
//   - 面板双防 = ENTER_EXIT_STAGE early 节点（epoch 防重，重登场重施——K190 形）：
//     numericalBase[DEFENSE]/[SPECIAL_DEFENSE] += max_hp×db[0]/100（k_panel 面板写，
//     权威基线直加）；
//   - 低伤门免异常 = EVENT_TAKE_DAMAGE watcher arm-once（K1 766 门：ev.actor==own +
//     ATTACK_DAMAGE 时点）→ ws.raw_attack_damage < db[1] → grant_immunity(ANOMALY,
//     mask 0=全异常, duration 2, ON_STAGE)——⚠️ 时长校准（K245 记档：duration 计入
//     施加当回合）：AFTER_ACTION 施加、duration 2 → 覆盖下回合（R1 末授予 → R2 有效）；
//   - raw 台账 = ws.raw_attack_damage（"攻击技能造成的伤害"，K1 766 同源）；
//   - tips 无 boss 子句，不做 boss 门（记档）。
//
// 动工前置三查（2026-10-13，author k-prod-6 第三轮）：①内核 grep 775 无魂印行为
//   硬编码；②effect_icon kind=[7,11]、tips 为权威文；③registry 无 775、soul_lib
//   并集（hub+feat/k226-k243）无 775——真缺口。
// 先例：k_panel（numericalBase 面板写）、K190（grant_immunity ANOMALY + enter epoch 形）、
//   K1 766/K239（攻击窗 + raw 台账）。
// 场景回归：test/scenario/scenario_3990_panel_def_lowraw_immune_775.cpp（号段 3990-3999）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 775;
constexpr int kWiredKeyBase = 991448000;   // +owner：watcher 已挂旗标

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 宿主行参数（"行≠宏"，K226 同款私副本）────────────────────────────────────

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

// ── early：面板双防（epoch 防重）+ 低伤门免异常 watcher ─────────────────────

EffectResult soulmark_775_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int def_pct = db_at(row, 0, -1);
    const int raw_gate = db_at(row, 1, -1);
    if (def_pct <= 0 || raw_gate <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    // early 双触发防重（K184/K197 EpochGuard 同因）：activate + init_battle 各跑一次
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ElfPet& self = ctx->getPet(owner);
    // ① 面板双防 += 体力×db[0]%（k_panel 权威基线直加）
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp > 0) {
        const int delta = max_hp * def_pct / 100;
        self.numericalBase[NumericalPropertyIndex::DEFENSE] += delta;
        self.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE] += delta;
    }
    // ② 低伤门免异常 watcher（arm-once）
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, raw_gate](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.amount <= 0) {
                return;
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const int raw = wctx->ws.raw_attack_damage[owner];
            if (raw < 0 || raw >= raw_gate) {
                return;   // 低伤门：raw < db[1]（严格小于）
            }
            // duration 2 = 计入施加当回合（K245 记档）→ 覆盖下回合
            wctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                 /*anomaly_mask=*/0, /*duration_rounds=*/2,
                                 /*source_id=*/kMark, /*soul_immunity=*/false,
                                 EffectScope::ON_STAGE, /*counts=*/0,
                                 /*source_effect_id=*/kMark, ImmunityTier::Modern);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：775）。early=面板双防 + 低伤门免异常 watcher。
SOUL_PROGRAM(775, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_775_arm,
                    /*once=*/false, /*early=*/true)});
