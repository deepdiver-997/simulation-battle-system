// K245 · 低体麻痹+高体增伤窗族 —— effect 798（1 宏 2 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k245）
//
// 行清单（tools/soul_effects_dump.py 798，2026-10-13）：
//   798 row492（格劳瑞 3150）args[2,2,50,1]
//     「回合开始时，若自身体力低于1/2，则回合结束后使对手麻痹2回合；|战斗阶段结束时，
//     若自身体力高于1/2，则下回合自身造成的伤害提高50%（boss无效）」
//   798 row528（同载体 3150）args[2,2,75,1]
//     同文；db[2]=增伤 75%（行间互锚唯一变量）。
//   → db[0]=低体门分母（< max/db[0]）；db[1]=高体门分母（> max/db[1]）；
//     db[2]=下回合增伤%；db[3]=1 不消费（K2 纪律记档）。tips 为权威文（SOP §六.1）。
//
// 实现口径（立单行模板，k-prod-5 拍板）：
//   - ROUND_START：双门快照（低体 < max/db[0] → low 账本；高体 > max/db[1] →
//     high 账本；int 除法严格边界）；
//   - ROUND_END low 账本 → g_core->apply_anomaly 麻痹直附 2 回合（码 0，100% 档
//     绕开 checked——立单行拍板；免疫/抗性仍由原语自查）；
//   - ROUND_END high 账本 → pending = roundCount+1（1576 "下回合"账本形）；
//     FMR：pending==roundCount → ws.damage_add_pct[owner] += db[2]（battleContext.h
//     :1594 钦定通道：ws 每回合 reset 天然清空，+= 等价独立窗，不与他效果互踩）；
//   - 门为 int 除法严格边界（< 与 >，恰在 max/db 不触发）；
//   - （boss无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-13）：①内核 grep 798 无魂印行为硬编码；②effect_icon
//   kind=[1,10]、tips 为权威文（dump 原文已核）；③registry 无 798、soul_lib 并集
//   （hub+feat/k226-k243）无 798——真缺口。
// 先例：K229（ROUND_START 快照账本）、K239（"下回合"pending+FMR 消费）、
//   1576（damage_add_pct 回合写入钦定形）、K226（ROUND_END 形）、miaoshi（apply_anomaly 直附）。
// 场景回归：test/scenario/scenario_3920_lowgate_para_highgate_amp_798.cpp（号段 3920-3929）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 798;
constexpr int kLowGateBase = 991445000;      // +owner：低体账本（1/0）
constexpr int kHighGateBase = 991446000;     // +owner：高体账本（1/0）
constexpr int kAmpRoundBase = 991447000;     // +owner：增伤回合 pending

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

// ── ROUND_START：双门快照 ───────────────────────────────────────────────────

EffectResult soulmark_798_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int low_denom = db_at(row, 0, -1);
    const int high_denom = db_at(row, 1, -1);
    if (low_denom <= 0 || high_denom <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0 || self.hp <= 0) {
        ctx->plugin_storage[kLowGateBase + owner] = 0;
        ctx->plugin_storage[kHighGateBase + owner] = 0;
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kLowGateBase + owner] = self.hp < max_hp / low_denom ? 1 : 0;
    ctx->plugin_storage[kHighGateBase + owner] = self.hp > max_hp / high_denom ? 1 : 0;
    return EffectResult::kOk;
}

// ── ROUND_END：低体直附麻痹 2 回合 + 高体置增伤 pending ─────────────────────

EffectResult soulmark_798_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int amp_pct = db_at(row, 2, -1);
    if (amp_pct < 0) {
        return EffectResult::kOk;
    }
    const auto lit = ctx->plugin_storage.find(kLowGateBase + owner);
    if (lit != ctx->plugin_storage.end() && any_int_or(lit->second, 0) == 1) {
        ctx->plugin_storage.erase(lit);
        if (g_core && g_core->apply_anomaly) {
            (void)g_core->apply_anomaly(ctx, 1 - owner,
                                        static_cast<int>(AbnormalStatusId::Paralysis),
                                        /*rounds=*/2, owner);
        }
    }
    const auto hit = ctx->plugin_storage.find(kHighGateBase + owner);
    if (hit != ctx->plugin_storage.end() && any_int_or(hit->second, 0) == 1) {
        ctx->plugin_storage[kAmpRoundBase + owner] = ctx->roundCount + 1;
    }
    return EffectResult::kOk;
}

// ── FMR：消费增伤 pending → ws.damage_add_pct ──────────────────────────────

EffectResult soulmark_798_fmr(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kAmpRoundBase + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, -1) != ctx->roundCount) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(it);
    const HostRow row = host_row(ctx, args, kMark);
    const int amp_pct = db_at(row, 2, 50);
    ctx->ws.damage_add_pct[owner] += amp_pct;   // 钦定通道：ws 每回合 reset，+= 独立窗
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：798）。ROUND_START=双门快照；ROUND_END=麻痹直附+pending；FMR=增伤写入。
SOUL_PROGRAM(798, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_798_round_start,
                    /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_798_round_end,
                    /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_798_fmr,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});
