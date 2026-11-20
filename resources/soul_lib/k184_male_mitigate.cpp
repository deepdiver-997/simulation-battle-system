// K184 雄性来源减伤族 —— effect 272（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 272，2026-10-07）：
//   272 貂蝉 2479 args[20, 0] / args[25, 0]：「减少20%来自雄性精灵的攻击伤害」/
//     「减少25%（进化行，同构互锚）」
//     → db[0]=减伤百分比（20/25 进化）、db[1] 恒 0（官方 tips 无对应子句，不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：k_guard_family arm_percent_mitigate 同形态——
//    REDUCE_PCT + MITIGATE 红伤票（register_damage_effect，ON_STAGE epoch 绑定，
//    换宠重上场由 enter 节点重挂）；**新增攻方性别门**：d.attackerId 侧在场宠
//    gender == MALE（枚举 MALE=0/FEMALE=1/NONE=2，monsters.gender DB 实证）。
//    EpochGuard 防开局 early 双触发（activate + init_battle 桶各一次）同 epoch 重注册。
//
// ⚠️ 三查记录（author 2026-10-07 / k-prod-2 复核同日）：①内核无 272 魂印行为硬编码
//    ②effect_info 无 272 技能模板（技能空间无撞号）③soul_lib 并集无同 id 注册——真缺口。
// 先例：k_guard_family（arm_percent_mitigate/EpochGuard 全形态，3 直系）。
// 场景回归：test/scenario/scenario_3160_male_mitigate.cpp（号段 3160-3169）。

#include "common.h"

#include <entities/elf-pet.h>

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
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


constexpr int kMarkId = 272;

// arm 幂等守卫（k_guard_family EpochGuard 私副本）：early 节点开局跑两次（activate +
// init_battle 桶），同 epoch 只挂一次；换宠重上场 epoch 递增自然重挂。
struct EpochGuard {
    int armed_epoch = -1;
};

bool already_armed_this_epoch(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(kMarkId)) {
        pet.soulmark_storage[kMarkId] = EpochGuard{};
    }
    EpochGuard& guard = std::any_cast<EpochGuard&>(pet.soulmark_storage[kMarkId]);
    if (guard.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    guard.armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

EffectResult k184_272_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || already_armed_this_epoch(ctx, a.owner)) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    // 行参走类型化 SoulNodeArgs（T4）：row(0)=减伤百分比。
    const HostRow row = host_row(ctx, a, 272);
    const int pct = db_at(row, 0, 20);
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
            if (!onstage_has_soul_mark(c, bucket_owner, kMarkId)) {
                return;
            }
            // 雄性来源门：攻击方在场宠 gender == MALE。
            const ElfPet& attacker =
                c->seerRobot[d.attackerId].elfPets[c->on_stage[d.attackerId]];
            if (attacker.gender != Gender::MALE) {
                return;
            }
            d.final = d.final * (100 - pct) / 100;
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（272 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k184_272_arm,
                   /*once=*/false, /*early=*/true)});
