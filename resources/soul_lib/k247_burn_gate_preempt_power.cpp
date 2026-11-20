// K247 · 对手烧伤门先制+威力翻倍族 —— effect 739（1 宏 1 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k247）
//
// 行清单（tools/soul_effects_dump.py 739，2026-10-13）：
//   739 row416（鬼焰神君陆逊 3066）args[2,1]（kind[1,9]，单行）
//     「若对手处于烧伤状态则自身所有技能先制+1并且攻击威力翻倍」
//   → db[0]=威力翻倍倍数（×2）；db[1]=先制+1。tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   - 先制+1 = BATTLE_FIRST_MOVE_RIGHT 节点：has_anomaly(对手, Burn)（单 id 查，
//     g_core 槽）→ ws.preemptive_level[owner] += db[1]（K227 FMR 形；⚠️ 首跑记档：
//     ROUND_START 写会被 FMR 入口 memset 清零——1843A 的 ROUND_START 写法在本内核
//     不成立，桶序=memset 先于 FMR 桶）；
//   - 威力翻倍 = BATTLE_FIRST/SECOND_SKILL_EFFECT 节点（SOP §二.3：威力改写必须在
//     SKILL_EFFECT 时点——636 直系同款 roundChoice+类型门）：对手处于烧伤 →
//     ws.skill_power_view[owner] = view.value × db[0]（只翻攻击技）；
//   - 条件查询用 burn 而非睡眠类（burn 不因受击清除，SKILL_EFFECT 时点查询无
//     K1 过期问题）；
//   - （BOSS无效）：tips 无此子句——本行 tips 无 boss 标注，不做 boss 门（记档）。
//
// 动工前置三查（2026-10-13，author k-prod-6 第三轮）：①内核 grep 739 无魂印行为
//   硬编码；②effect_icon kind=[1,9]、tips 为权威文；③registry 无 739、soul_lib
//   并集（hub+feat/k226-k243）无 739——真缺口。
// 先例：k_round_stat 636（SKILL_EFFECT 威力翻倍 view 形——条件异族直系）、K6 1843A
//   （ROUND_START 条件先制）、K239（has_anomaly 单 id 查）。
// 场景回归：test/scenario/scenario_3970_burn_gate_preempt_power_739.cpp（号段 3970-3979）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

namespace {

constexpr int kMark = 739;

bool has_burn(BattleContext* ctx, int side) {
    return g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, side,
                                      static_cast<int>(AbnormalStatusId::Burn));
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

// ── FMR：对手烧伤门 → 先制 +db[1]（K227 形）────────────────────────────────
// ⚠️ 首跑记档：ROUND_START 写 preemptive_level 会被 handle_BattleFirstMoveRight
// 入口 memset 清零（先 memset 后跑 FMR 桶）——先制写入必须挂 FMR 桶（K227 同款）。

EffectResult soulmark_739_fmr(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int preempt = db_at(row, 1, -1);
    if (preempt <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    if (!has_burn(ctx, 1 - owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += preempt;
    return EffectResult::kOk;
}

// ── SKILL_EFFECT：对手烧伤门 → 攻击威力 ×db[0]（636 直系）───────────────────

EffectResult soulmark_739_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技的威力
    }
    if (!has_burn(ctx, 1 - owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int mult = db_at(row, 0, 2);
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0 && mult > 1) {
        view = view.value * mult;
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：739）。FMR=烧伤门先制（memset 后写）；FIRST/SECOND_SKILL_EFFECT=威力翻倍。
SOUL_PROGRAM(739, {SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_739_fmr,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT,
                    &soulmark_739_skill_effect, /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT,
                    &soulmark_739_skill_effect, /*once=*/false, /*early=*/false)});
