// K335 战阶磨血（无下降门）族 —— effect 655（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 655，2026-10-21）：
//   655 神武圣皇 2922 args[1,4,1]（kind=[]）：「战斗阶段结束时若对手不处于能力下降
//     状态则每回合减少1/4最大HP（boss无效）」
//     → **db[1] = 1/4 的分母**（tips 字面 "1/4最大HP" ↔ 4；K 板 359「减少1/5最大
//       HP」args[2,2,5] 的 db[2]=5 同款分母位口径）；db[0]=1 / db[2]=1 语义存疑
//       不消费、记档（tips 无数值子句与之对应）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = **BATTLE_ROUND_END**（"战斗阶段结束时"——K258 359 / K226 / K 板 332 /
//     chaos_sorensen / k178 351 / k186 六处同锚；ROUND_END 桶**双方都跑**）。
//   门 = **对手不处于能力下降状态** = 对手任一能力槽 ability_levels[opp][i] < 0
//     的**否定**（即无任一负等级）——引擎口径"处于能力下降状态" = 存在负等级
//     （k165_gate_attach_power 94 / k206_cond_heal 25 同款判据，取反；
//     K258 359 的"不处于能力提升"门是同款取反结构）。
//   效果 = deal_pink_damage(对手, maxHP/db[1], FIXED)（"减少1/4最大HP"=
//     FIXED 档粉伤，吃护罩；K 板 359 / K260 332 同款口径）。
//     ⚠️ 边界：对手已倒（hp<=0）/ 已下场时磨血空转（K258 359 / K268 796 同款守卫）。
//   ⚠️ boss 无效：tips 尾注带（boss无效）→ 节点 boss_invalid 置位。
//
// ⚠️ 三查记录（author a13-kprod 2026-10-21）：①内核 grep 655 无魂印行为硬编码；
//    ②effect_icon kind=[]、tips 为权威文（dump 原文已核，effect_info 无 655
//    技能模板）；③registry soul 集合无 655、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：K258 359（战阶结束减少 1/db[2] 最大HP 直系，旗舰）、K260 332（ROUND_END
//   粉伤 FIXED）、k165_gate_attach_power（能力下降门判据）、k178 351（分母位口径）。
// 场景回归：test/scenario/scenario_4910_nodrop_grind_655.cpp（号段 4910-4919）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 655;
constexpr int kDenomFallback = 4;   // tips 字面 1/4 的分母（db[1] 读不到时回退）

// 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）。
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

// 对手是否处于能力下降状态（引擎口径 = 任一能力槽为负等级）。
bool opp_has_stat_drop(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// ROUND_END：对手不处于能力下降状态 → 对手减少 1/db[1] 最大HP。
EffectResult k335_655_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    // 门：对手**不处于**能力下降状态（无任一负等级）。
    if (opp_has_stat_drop(ctx, opp)) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：磨血空转（K258 359 同款守卫）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 1, kDenomFallback);
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

// ── 唯一注册（655；K312/K335 同行实现已收敛，保留唯一ID覆盖）─────────────
// （boss无效）尾注 → boss_invalid 置位。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k335_655_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
