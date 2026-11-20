// K5 闪避/命中率常驻数值族 —— effect 7/725/1502（3 宏 3 行）
//（260/280 的闪避子句补全在 k_guard_family.cpp——SOUL_PROGRAM 注册体在那边，只增不改）
//
// 行清单（tools/soul_effects_dump.py 7 725 1502，2026-10-04）：
//   7    傲世战魂 2553 args空   「使对手的命中率-6%」
//        → hit_rate_mod[opp]=0.94（ROUND_START 每回合写，476 圣芒套路）
//   725  焱武·圣尊 3043 args[50,50] 「受到的伤害减少50%,对方所有技能命中率降低50%」
//        → 红伤 REDUCE_PCT（"受到的伤害"措辞 = K1 3/280 同款仅红伤）+ hit_rate_mod[opp]=0.5
//   1502 追击者M3 3911 args[1,50,1] 「每回合自身速度+1，自身处于速度提升状态时闪避率提升50%」
//        → ROUND_START stat_change(速槽4, +db[0]) + view_levels[速]>0 门 → dodge_rate=self db[1]%
//
// S1-P0 命中/闪避钩子消费首波（K 工单 2026-10-01 追加节解锁）：ws.hit_rate_mod
// （全技命中率乘算）、ws.dodge_rate（减算 hit_chance = accuracy - dodge×100，每回合
// reset 归零）。命中率百分比是面板乘区，能力等级槽 5（±6 等级制）表达不了 → 走 ws 槽，
// 常驻魂印每回合 ROUND_START 重写。时点存活依据：resetWorkspace 在 ROUND_START 状态
// **先**执行、时点桶**后**执行（battleFsm handle_BattleRoundStart :1669 → :1691 序），
// 本族 ROUND_START 节点写的槽位不会被本回合 reset 冲掉（场景 962 注入记档同源）。
//
// ⚠️ effect 7 无参：单行、数值取行文字面（-6% → ×0.94）。725/1502 读 db 参数驱动。
//
// 三查记录（2026-10-04）：
//   ① 内核无 7/725/1502 硬编码（grep src/include 零命中）；
//   ② effect_info 同 id 是技能空间模板，与魂印空间无关（K1 ②同款口径）；
//   ③ registry soul=75 无三 id；未合并分支（web-console-ux/local/main/verify）无实现。
#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 宿主行参数视图（k_guard_family/k_round_stat 同款，各文件自治）────────────

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

// arm 幂等守卫（K1 EpochGuard 同款语义，int 槽版）：开局 early 节点双触发防重，
// 换宠 epoch 递增自然重挂。键位被异类型占用 → 视为已武装（共享键空间防误重挂，
// common.h any_cast 治理口径：不抛、按缺失/已占语义回退）。
// ⚠️ 先 find 后 insert：operator[] 会插入空 any，紧跟的"已占用"判定就恒真（首跑事故）。
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

// ── effect 7：对手命中率 -6%（常驻光环）──────────────────────────────────────

EffectResult soulmark_7_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 7)) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[1 - owner] = 0.94f;   // -6%，行文字面（args 空）
    return EffectResult::kOk;
}

// ── effect 725：受伤 -50%（红伤）+ 对手所有技能命中率 -50% ───────────────────

EffectResult soulmark_725_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 725 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 725);
    const int pct = db_at(row, 0, row.monster_id == 3043 ? 50 : -1);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 725)) {
                return;
            }
            d.final = d.final * (100 - pct) / 100;
        });
    return EffectResult::kOk;
}

EffectResult soulmark_725_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 725)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 725);
    const int pct = db_at(row, 1, row.monster_id == 3043 ? 50 : -1);
    if (pct > 0) {
        ctx->ws.hit_rate_mod[1 - owner] = 1.0f - pct / 100.0f;   // "所有技能"= 全技乘区
    }
    return EffectResult::kOk;
}

// ── effect 1502：每回合速度+1；速度提升态闪避 +50% ──────────────────────────

EffectResult soulmark_1502_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1502)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 1502);
    const int speed_up = db_at(row, 0, 1);
    const int dodge_pct = db_at(row, 1, row.monster_id == 3911 ? 50 : -1);
    if (speed_up > 0 && g_core && g_core->stat_change) {
        g_core->stat_change(ctx, owner, kAbilityLevelIndexSpeed, speed_up);
    }
    // "处于速度提升状态" = 速度等级视图 > 0（stat_change 本体+视图三合一，此处即见增量）。
    if (dodge_pct > 0 && ctx->ws.view_levels[owner][kAbilityLevelIndexSpeed] > 0) {
        ctx->ws.dodge_rate[owner] = dodge_pct / 100.0f;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（只增不改）────────────────
SOUL_PROGRAM(7, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_7_round_start)});
SOUL_PROGRAM(725, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_725_arm,
                   /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_725_round_start)});
SOUL_PROGRAM(1502, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1502_round_start)});
