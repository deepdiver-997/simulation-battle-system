// K229 · 血量对照减伤+随机吸取族 —— effect 1422（1 宏 2 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k229）
//
// 行清单（tools/soul_effects_dump.py 1422，2026-10-08）：
//   1422 row1108（魂王·暗翼影 3833）args[30,40,1,300,150,3,1,1]
//     「回合开始时，若自身体力高于对手则受到的攻击伤害减少30%，若自身体力低于对手则受
//     到的攻击伤害减少40%；战斗阶段结束时随机{吸取}对手1-300点{体力}，若{吸取}的{体力}
//     低于150点则对手随机3项能力值-1（boss无效）」
//   1422 row1622（同载体 3833，奉身铸魂伙伴效果）args[30,40,100,300,200,3,1,1]
//     同文；db[2]=吸取下限 100、db[4]=降属阈值 200（行间互锚唯一变量）。
//   → db[0]=体力**高于**对手的减伤%；db[1]=**低于**对手的减伤%；db[2]=吸取下限；
//     db[3]=吸取上限；db[4]=降属阈值（实吸低于它才降）；db[5]=随机降属项数；
//     db[6]=降属幅度（两行均 1，与文本"-1"一致）；db[7] 不消费（K2 纪律：解析不了
//     宁可不用，不猜）。tips 为权威文（SOP §六.1；effect_icon kind=[5,12]）。
//
// 实现口径：
//   - 减伤票 = REDUCE_PCT + DamageEffectCategory::MITIGATE（K1 3/280 同构），只红伤
//     （官方"受到的**攻击**伤害"——粉伤/真伤不沾，isRed 门）；
//   - 减伤判定 = **回合开始时快照**（官方文案锚"回合开始时"）：ROUND_START 节点估
//     高于/低于/相等 → 账本写 db[0]/db[1]/0（plugin_storage，K7 账本显式写/清模式），
//     票只在伤害时读账本、不重估——回合内 HP 反超不改本回合档位；相等两门都不满足；
//   - 吸取 = BATTLE_ROUND_END（=官方"战斗阶段结束时"，K226/chaos_sorensen/k178/k186/
//     puni_1038 五处同锚）：rand ∈ [db[2],db[3]]（k893 sbs_rand 同款）；实吸量取
//     HP 差真值（deal_pink_damage 吃护罩/免粉且返回值无实际量，"吸取的体力"=实际
//     掉血），heal_amount 按实吸回（k225 记档差异：彼处按名义值回）；
//   - 降属 = 实吸 < db[4] → rand%6 去重选槽 ×db[5] 项 stat_drop(对手, 槽, db[6])
//     （1314/893 同款循环）；对手被吸死也照降（stat_drop 无存活守卫，官方文本无条件式）；
//   - （boss无效）：按 K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08）：①内核 grep 1422 无硬编码（0 命中）；②effect_icon
//   kind=[5,12]、tips 为权威文（dump 原文已核）；③soul_lib 无 1422 注册，sibling
//   分支 feat/k226/k227/k228 亦无（K167 撞车教训，全分支 grep）。
// 先例：K1 3/280（MITIGATE 票形）、K226 869（BATTLE_ROUND_END 锚+账本/审计槽+early
//   防重形）、K4/893·1314（随机去重降属）、K225 341（粉伤吸取双段）。
// 场景回归：test/scenario/scenario_3730_hpcmp_drain_1422.cpp（号段 3730-3739）。

#include "common.h"

#include <cstdlib>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1422;
constexpr int kDrainChanceSite = 0;
constexpr int kDropSlotChanceSite = 1;
constexpr int kWiredKeyBase = 991420000;   // +owner：减伤票已挂旗标（防 early 双触发）
constexpr int kLedgerKeyBase = 991421000;  // +owner：本回合减伤%（ROUND_START 写，票读）
constexpr int kDrainKeyBase = 991422000;   // +owner：本回合实吸量（审计槽，F9 口径，只读）

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

// ── early：挂一次减伤票（pct 动态读账本）────────────────────────────────────

EffectResult soulmark_1422_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（K226 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMark)) {
                return;
            }
            int pct = 0;
            const auto it = c->plugin_storage.find(kLedgerKeyBase + bucket_owner);
            if (it != c->plugin_storage.end()) {
                pct = any_int_or(it->second, 0);
            }
            if (pct <= 0) {
                return;   // 相等/未估 → 本回合无减伤
            }
            d.final = d.final * (100 - pct) / 100;
        });
    return EffectResult::kOk;
}

// ── ROUND_START：估"体力高/低于对手"→ 账本写 db[0]/db[1]/0 ──────────────────

EffectResult soulmark_1422_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int pct_high = db_at(row, 0, -1);
    const int pct_low = db_at(row, 1, -1);
    if (pct_high < 0 || pct_low < 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(1 - owner);
    int pct = 0;
    if (self.hp > foe.hp) {
        pct = pct_high;
    } else if (self.hp < foe.hp) {
        pct = pct_low;
    }
    ctx->plugin_storage[kLedgerKeyBase + owner] = pct;
    return EffectResult::kOk;
}

// ── ROUND_END（战斗阶段结束）：随机吸取 + 阈值门随机降属 ─────────────────────

EffectResult soulmark_1422_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int lo = db_at(row, 2, -1);
    const int hi = db_at(row, 3, -1);
    const int gate = db_at(row, 4, -1);
    const int drop_cnt = db_at(row, 5, -1);
    const int drop_amt = db_at(row, 6, 1);
    if (lo < 0 || hi < lo || gate < 0 || drop_cnt <= 0 || drop_amt <= 0) {
        return EffectResult::kOk;   // 参数不齐不武装（记档）
    }
    const int opp = 1 - owner;
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(opp);
    if (self.hp <= 0 || foe.hp <= 0) {
        return EffectResult::kOk;   // 阵亡不吸
    }
    const int hp_before = foe.hp;
    const int roll = choose_soul_range(*ctx, kMark, kDrainChanceSite, owner, opp, lo, hi);
    if (g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, opp, roll, DamageKind::PERCENT_VALUE, owner);
    }
    const int drained = hp_before - ctx->getPet(opp).hp;   // 实吸真值（护罩/免粉后）
    ctx->plugin_storage[kDrainKeyBase + owner] = drained;  // 审计槽（场景无随机断言用）
    if (drained > 0 && g_core && g_core->heal_amount) {
        (void)g_core->heal_amount(ctx, owner, drained);
    }
    if (drained >= gate || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;   // 阈值门：低于才降属
    }
    int picked[6] = {};
    int done = 0;
    int guard = 0;
    while (done < drop_cnt && guard < 100) {
        ++guard;
        const int slot = choose_soul_range(*ctx, kMark, kDropSlotChanceSite, owner, opp, 0, 5);
        if (picked[slot]) {
            continue;
        }
        picked[slot] = 1;
        ++done;
        (void)g_core->stat_drop(ctx, opp, slot, drop_amt, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1422）。early=减伤票一次挂；ROUND_START=账本重估（更新器每回合重注册，
// SOP §一.1）；ROUND_END=吸取/降属节点。
SOUL_PROGRAM(1422, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1422_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1422_round_start,
                    /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1422_round_end,
                    /*once=*/false, /*early=*/false)});
