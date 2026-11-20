// K258 对手无提升→战阶磨血族 —— effect 359（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 359，2026-10-21）：
//   359 吉罗德 2752 args[2,2,5]（kind=[]）：「若对手能力不处于提升状态时，则每个
//     战斗阶段结束时减少对手1/5最大HP（boss无效）」
//     → **db[2] = 分母**（1/5，tips 字面；同族 351 args[1000,5] 的 db[1]=5 同款
//       "分母位"）；db[0]/db[1]=2 语义存疑不消费（K2 纪律：解析不了宁可不用、
//       不猜）——注意**分母位不通用**：351 的分母在 db[1]、655（args[1,4,1]）在
//       db[1]、359 在 db[2]，故本单按 tips 字面 1/5 落地并以 db[2] 为读值，
//       tips 字面 5 作回退。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   门 = **对手能力不处于提升状态** = 对手任一能力槽 ability_levels[opp][i] > 0
//     的**否定**（即无任一正等级）——引擎口径"处于能力提升状态" = 存在正等级
//     （k172_round_start_clear_fear 808 的 opp_has_boost 同款判据，取反）。
//   时点 = BATTLE_ROUND_END（"每个战斗阶段结束时"——K178 351 同锚：战斗阶段结束
//     = 回合结束，K226/chaos_sorensen/k178/k186/puni_1038 五处同锚）。
//     ⚠️ 与 351 的差别：351 的门在"使用攻击技能"（EVENT_SKILL_USED 窗）+ 计数，
//     359 是**每回合无条件**（只要门开）→ 不需要窗/计数，单节点即可。
//   效果 = deal_pink_damage(对手, maxHP/db[2], FIXED, 印主)（粉伤通道，K10 637
//     反弹 / K178 351 同款调用形）——粉伤吃护罩/护甲，官方"减少X最大HP"走粉伤
//     与 351 一致。
//   ⚠️（boss无效）尾注 → 节点 boss_invalid 置位（boss 挑战对局不注册不执行）。
//   ⚠️ 边界：对手当回合 hp≤0 / 已下场时磨血空转（K178 351 同款守卫）。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 359 无魂印行为硬编码；
//    ②effect_icon kind=[]、tips 为权威文（dump 原文已核，effect_info 无 359
//    技能模板）；③registry soul 集合无 359、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k178_battle_stage_grind 351（战斗阶段结束扣 1/db[1] 最大HP 族，旗舰）、
//   k172_round_start_clear_fear 808（能力提升门判据）、K226（ROUND_END 锚）。
// 场景回归：test/scenario/scenario_4140_noboost_grind_359.cpp（号段 4140-4149）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 359;
constexpr int kDenomFallback = 5;   // tips 字面 1/5 的分母（db[2] 读不到时回退）

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）─────────
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

// 对手是否处于能力提升状态（引擎口径 = 任一能力槽为正等级）。
bool opp_has_boost(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

// ROUND_END：对手不处于能力提升状态 → 对手减少 1/db[2] 最大HP。
EffectResult k258_359_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    // 门：对手**不处于**能力提升状态（= 无任一正等级）。
    if (opp_has_boost(ctx, opp)) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：磨血空转（K178 351 同款守卫）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 2, kDenomFallback);
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const int grind = resolve_pet_max_hp(foe) / denom;
    if (grind <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, grind, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（359 入账，只增不改）──────
// （boss无效）尾注 → boss_invalid 置位。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k258_359_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
