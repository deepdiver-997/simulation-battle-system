// K205 满血增伤族 —— effect 325（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 325，2026-10-07）：
//   325 神谕龙骑 2665 args[10, 0]：「满HP时造成的伤害提升10%」
//     → db[0]=增伤百分比（10）、db[1] 恒 0（官方 tips 无对应子句，不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：SKILL_EFFECT 时点 view 增伤（K187/291 直系形态，
//    FIRST/SECOND 双桶同函数）：roundChoice 门 + 类型表门（该时点 skill_use_seq 未
//    自增，K4 判据）→ **满血门**（hp == resolve_pet_max_hp）→
//    skill_power_view[owner] ×(100+db[0])/100。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 325 魂印行为硬编码 ②effect_info
//    无 325 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k187_shield_power（SKILL_EFFECT view 增伤全形态）、k165（291 原型）。
// 场景回归：test/scenario/scenario_3445_fullhp_power.cpp（号段 3445-3454）。

#include "common.h"

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


constexpr int kMarkId = 325;

EffectResult k205_325_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5
        || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只增攻击技（K4 判据：类型表门）
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0 || self.hp != resolve_pet_max_hp(self)) {
        return EffectResult::kOk;   // 满血门
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        const HostRow row = host_row(ctx, a, 325);
        view = view.value * (100 + db_at(row, 0, 10)) / 100;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（325 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT,
                             &k205_325_skill_effect),
                       SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT,
                                 &k205_325_skill_effect)});
