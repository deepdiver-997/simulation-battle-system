// K280 技能类型门自增益族 —— effect 884（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 884，2026-10-21）：
//   884 极地·沃德斯特 3271 args[2,2]（kind=[11]）：「使用攻击技能时自身防御+2；
//     使用属性技能时自身特防+2」
//     → **db[0] = 攻击技时的防御增量 2**（tips 字面 "防御+2" ↔ 2，防御槽 = 2）；
//     **db[1] = 属性技时的特防增量 2**（tips 字面 "特防+2" ↔ 2，特防槽 = 3）。
//     两参数与文本**完全自洽** → 均消费。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = **BATTLE_FIRST/SECOND_SKILL_EFFECT**（技能效果结算时点）——本子句要判
//     "用的是攻击技还是属性技"，需读**所选技的静态类型**：roundChoice[owner][1]
//     取槽下标 → getPet(owner).skills[slot].type != SkillType::Attribute
//     （shengguang_gelaorui 2532 同款判读；K4 636 的"每回合 stat 对"同款时点）。
//     ⚠️ 不用 EVENT_SKILL_USED watcher：K259 345 证明"使用即发"可用，但 watcher
//     回调时 roundChoice 可能已被回合末重置（K261 373 实测：回合末读 roundChoice
//     得 -1）→ 技能类型判读**必须**在技能效果时点做。
//   门 = 印主本回合**确实出了技能**（roundChoice[owner][0] == SELECT_SKILL）且
//     槽下标合法（0..4）——否则（NONE/药剂/被控跳过）不触发。
//   效果 = stat_change(印主, 槽, db[0]/db[1])（自身增益通道，**不查免弱**——
//     stat_drop 才查免弱；K255 320 / K265 382 / K 板 622 同款自增益写法）。
//     · 攻击技 → 防御槽 2，+db[0]
//     · 属性技 → 特防槽 3，+db[1]
//     ⚠️ 等级上限 ±6 由原语内钳。
//
// ⚠️ 三查记录（author a09-kprod 2026-10-21）：①内核 grep 884 无魂印行为硬编码
//    （moves_lib 的 884 是**技能空间**同号：j_lowdmg_gate3「884/1293 次数型封技
//    可穿族口径」——不同注册空间，不算已实现）；②effect_icon kind=[11]、tips 为
//    权威文（dump 原文已核，effect_info 无 884 技能模板）；③registry soul 集合
//    无 884、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：shengguang_gelaorui 2532（技能类型判读 + 槽下标读法）、k_round_stat 636
//   （SKILL_EFFECT 时点 stat 对）、K265 382（stat_change 自增益）。
// 场景回归：test/scenario/scenario_4360_skilltype_stat_884.cpp（号段 4360-4369）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 884;
constexpr int kDefenseSlot = 2;   // 能力等级槽 2 = 防御
constexpr int kSpDefenseSlot = 3; // 能力等级槽 3 = 特防
constexpr int kDefDeltaFallback = 2;   // tips 字面"防御+2"（db[0] 读不到时回退）
constexpr int kSpDefDeltaFallback = 2; // tips 字面"特防+2"（db[1] 读不到时回退）

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

// SKILL_EFFECT：按本回合所选技的类型给对应防御槽自增益。
EffectResult k280_884_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // 门：印主本回合确实选了技能。
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return EffectResult::kOk;
    }
    const bool is_attack = skills[static_cast<std::size_t>(slot)].type != SkillType::Attribute;
    const HostRow row = host_row(ctx, a, kMarkId);
    if (is_attack) {
        const int delta = db_at(row, 0, kDefDeltaFallback);
        if (delta != 0) {
            (void)g_core->stat_change(ctx, owner, kDefenseSlot, delta);
        }
    } else {
        const int delta = db_at(row, 1, kSpDefDeltaFallback);
        if (delta != 0) {
            (void)g_core->stat_change(ctx, owner, kSpDefenseSlot, delta);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（884 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k280_884_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k280_884_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
