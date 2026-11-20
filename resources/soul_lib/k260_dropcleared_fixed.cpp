// K260 对手能力下降被解除→固伤族 —— effect 332（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 332，2026-10-21）：
//   332 玄元风神 2680 args[150,0]（kind=[14]）：「若对手能力下降被解除，则战斗阶段
//     结束时直接给对手造成150点固定伤害」
//     → **db[0] = 固伤值 150**（tips 字面，与 db[0] 一致）；db[1]=0 语义存疑不消费
//       （记档；tips 无数值子句与之对应）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   触发源 = **快照比对**（无现成事件）：`clear_stat_drops`（消除能力下降的原语）
//     **不发任何事件**——只有 clear_stat_boosts / transfer_stat_boosts 成功路径
//     才 emit EVENT_STAT_BOOSTS_LOST（battle_primitives.cpp:1536/1673），
//     clear_stat_drops 走的是 write_ability_level 静默清零（:1591）。
//     故按 K222（338 对手等级变化回血）的**回合首快照 + 回合末比对**范式：
//       ①ROUND_START：快照对手 6 槽等级 + 记"快照时是否存在负等级"（has_drop 旗）。
//       ②ROUND_END：has_drop 为真 且 当前**无任何负等级** → 判定"能力下降在本回合
//         被解除" → deal_pink_damage(对手, db[0], FIXED)（"固定伤害"= FIXED 档粉伤，
//         K233 844 明确口径；吃护罩/粉伤免减）。
//     ⚠️ 口径记档：官方"被解除"指**本回合内从有到无**；若对手本回合先被下降、
//        后被解除，快照（回合首）时无负等级 → 本实现**不触发**（快照基准是回合首）。
//        若改为"回合内任意时刻解除"需逐时点记账，本单按回合首快照（K222 同款）。
//   时点 = BATTLE_ROUND_END（"战斗阶段结束时"——K226/chaos_sorensen/k178/k186/
//     puni_1038 五处同锚）。
//   ⚠️ 边界：对手已倒（hp≤0）/ 下场时固伤空转。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 332 无魂印行为硬编码；
//    ②effect_icon kind=[14]、tips 为权威文（dump 原文已核，effect_info 无 332
//    技能模板）；③registry soul 集合无 332、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k222_level_change_heal 338（回合首快照 6 槽 + 回合末比对，旗舰）、
//   k233_dualdef_fixed_heal 844（"固定伤害"= FIXED 档粉伤口径）、K226（ROUND_END 锚）。
// 场景回归：test/scenario/scenario_4160_dropcleared_fixed_332.cpp（号段 4160-4169）。

#include "common.h"

#include <array>

namespace {

constexpr int kMarkId = 332;
constexpr int kFixedFallback = 150;   // tips 字面固伤值（db[0] 读不到时回退）

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

// ── 账本：回合首快照 + "快照时有负等级"旗 ────────────────────────────────────
struct K260Runtime {
    int snapshot_round = -1;
    std::array<int, BattleContext::kAbilityLevelSlotCount> slots{};
    bool had_drop = false;   // 回合首快照时对手是否存在负等级
};

K260Runtime& k260_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K260Runtime{};
    }
    return std::any_cast<K260Runtime&>(storage[kMarkId]);
}

// ROUND_START：快照对手 6 槽等级 + 记"有无负等级"。
EffectResult k260_332_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    K260Runtime& rt = k260_runtime(ctx, owner);
    rt.snapshot_round = ctx->roundCount;
    rt.had_drop = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = ctx->ability_levels[opp][i];
        rt.slots[i] = lv;
        if (lv < 0) {
            rt.had_drop = true;
        }
    }
    return EffectResult::kOk;
}

// ROUND_END：回合首有下降、现在无下降 → 对手固伤 db[0]。
EffectResult k260_332_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K260Runtime& rt = k260_runtime(ctx, owner);
    if (rt.snapshot_round != ctx->roundCount || !rt.had_drop) {
        return EffectResult::kOk;   // 无本回合快照 / 回合首本就无下降
    }
    const int opp = ctx->opponent(owner);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return EffectResult::kOk;   // 仍有下降 → 未被解除
        }
    }
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：固伤空转
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int fixed = db_at(row, 0, kFixedFallback);
    if (fixed <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, fixed, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（332 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k260_332_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_END, &k260_332_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
