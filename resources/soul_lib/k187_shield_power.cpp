// K187 双方护盾梯度增伤族 —— effect 2474（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 2474，2026-10-07）：
//   2474 西格恩 4867 args[5, 1, 75]：「自身使用攻击技能时，双方每有5点护盾自身造成的
//     攻击伤害提升1%，最高75%，boss对战中上限翻倍（boss有效）」/ args[5, 2, 100]
//     （每5点+2%、最高100%，进化行同构互锚）
//     → db[0]=护盾梯度步长、db[1]=每步增伤%、db[2]=上限%。boss上限翻倍：记档不测
//     （author 口径，本单不做 boss 分支）。
//
// ⚠️ 实现形态（T4 类型化行参）：SKILL_EFFECT 时点 view 增伤（636/733/K185 同路，
//    K165 291 直系形态）：FIRST/SECOND 双桶同函数，roundChoice 门 + 技能表类型门
//    （该时点 skill_use_seq 未自增，executed_* 不可读——K4 判据）。
//    bonus = min(db[2], (own.shield_bank_.total() + opp.total()) / db[0] * db[1])；
//    写点 = 物化后、伤害公式前：skill_power_view[owner] ×(100+bonus)/100。
//
// ⚠️ 三查记录（author 2026-10-07 / k-prod-2 复核同日）：①内核无 2474 魂印行为硬编码
//    ②effect_info 无 2474 技能模板 ③soul_lib 并集无同 id 注册——真缺口；
//    ShieldBank::total() 实证 include/entities/shield_bank.h:116。
// 先例：k165_gate_attach_power（291 SKILL_EFFECT view 增伤全形态）、yanshou（shield_bank_）。
// 场景回归：test/scenario/scenario_3190_shield_power.cpp（号段 3190-3199）。

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


constexpr int kMarkId = 2474;

// ── SKILL_EFFECT 节点（FIRST/SECOND 双桶，持有方自身行动时触发）──
EffectResult k187_2474_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
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
    const HostRow row = host_row(ctx, a, 2474);
    const int per = db_at(row, 0, 5);
    const int step = db_at(row, 1, 1);
    const int cap = db_at(row, 2, 75);
    if (per <= 0) {
        return EffectResult::kOk;
    }
    // 双方护盾梯度：每 per 点 +step%，封顶 cap。
    const int total = ctx->getPet(owner).shield_bank_.total()
                    + ctx->getPet(1 - owner).shield_bank_.total();
    const int bonus = std::min(cap, total / per * step);
    if (bonus <= 0) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * (100 + bonus) / 100;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（2474 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT,
                             &k187_2474_skill_effect),
                       SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT,
                                 &k187_2474_skill_effect)});
