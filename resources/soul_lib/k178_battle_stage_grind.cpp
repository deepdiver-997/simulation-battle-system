// K178 攻击技战阶磨血族 —— effect 351（1 宏 / 2 行）
//                                                                          （2026-10-07，worktree ../agents/k178）
//
// 行清单（tools/soul_effects_dump.py 351，2026-10-07）：
//   351 扎尔瑟隆 2729 args[1000,5]「每次使用攻击技能都有100%概率使对手在战斗阶段结束时
//       减少1/5最大HP」/ 张辽 2770 args[500,5]「每次使用攻击技能都有50%概率降低对手
//       1/5最大HP（boss无效）」
//   → db[0]=**千分比**（1000→100%、500→50%，两行 tips 互锚；K164 151 同款 db/10 折算——
//     ⚠️ 概率单位逐宏互锚，严禁跨宏复用换算）；db[1]=分母（1/db[1] 最大HP，两行均 5）。
//   行级 boss 门：仅张辽行 tips 带（boss无效）→ 行参无 boss 位，按 monster_id==2770
//   判 is_boss_challenge（K178 看板行记档口径）。
//
// 实现（SoulMark 两节点）：
//   ① OPERATION_ENTER_EXIT_STAGE arm：注册 EVENT_SKILL_USED watcher（own+攻击技，
//     K164 109/116/151 同窗同门——miss 也算"使用"，E13 锚点语义）→ roll(db[0]/10 %)
//     → 置计数旗（soulmark_storage 990178001，一次使用一跳；额外行动多次使用多次计数）。
//   ② BATTLE_ROUND_END（"战斗阶段结束"）：消费计数 N → 对手扣 maxHP/db[1] × N
//     （deal_pink_damage FIXED 粉伤通道，K10 637 反弹同款调用形；对手 maxHP 现读
//     resolve_pet_max_hp）。
//   ⚠️ 边界记档：对手当回合 hp≤0/下场时磨血空转；50% 掷点发生在使用时点（每使一次掷
//     一次），非回合末合并掷。
//
// 三查记录（2026-10-07，立单时已核）：①内核 grep 351 无魂印行为硬编码；②effect_info
//   无 351 技能模板行（魂印以 effect_icon tips 为权威）；③soul_lib 并集无 351、
//   registry.soul 无 351——真缺口。
// 先例：K164 109/116/151（EVENT_SKILL_USED 攻击技窗+roll）、K10 637（deal_pink FIXED）、
//   K173 809（ROUND_END 消费旗）、K12 2920 口径（50% 档运行期统计断言）。
// 场景回归：test/scenario/scenario_3100_battle_stage_grind.cpp（T3100-T3103）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k351GrindCountKey = 990178001;   // 本回合"已使用攻击技且掷中"计数

// ── 宿主行参数（K164/K168 各文件自治同款）────────────────────────────────────
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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int flag_get(ElfPet& pet, int key) {
    auto it = pet.soulmark_storage.find(key);
    if (it == pet.soulmark_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

void flag_set(ElfPet& pet, int key, int value) {
    pet.soulmark_storage[key] = int{value};
}

// ── ① arm：攻击技使用窗 → 掷点置计数旗 ─────────────────────────────────────
EffectResult soulmark_351_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 351);
    const int permille = db_at(row, 0, -1);
    const int denom = db_at(row, 1, 5);
    if (permille <= 0 || denom <= 0) {
        return EffectResult::kOk;   // 无概率行不武装
    }
    // 行级 boss 门：张辽 2770 行 tips 带（boss无效）→ boss 挑战局不武装。
    if (row.monster_id == 2770 && ctx->is_boss_challenge) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, permille](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.used_attribute) {
                return;   // own+攻击技门（K164 同款；miss 也算使用）
            }
            if (!onstage_has_soul_mark(c, owner, 351)) {
                return;
            }
            if (!roll_percent(c->rng(), permille / 10)) {   // 千分比 → 百分比
                return;
            }
            ElfPet& host = c->seerRobot[owner].elfPets[c->on_stage[owner]];
            flag_set(host, k351GrindCountKey, flag_get(host, k351GrindCountKey) + 1);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ② 战斗阶段结束：消费计数 → 对手扣 1/db[1] maxHP × N ────────────────────
EffectResult soulmark_351_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 351)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    if (host.hp <= 0) {
        return EffectResult::kOk;
    }
    const int hits = flag_get(host, k351GrindCountKey);
    if (hits <= 0) {
        return EffectResult::kOk;
    }
    flag_set(host, k351GrindCountKey, 0);
    const HostRow row = host_row(ctx, args, 351);
    const int denom = db_at(row, 1, 5);
    ElfPet& foe = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (foe.hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：磨血空转
    }
    const int grind = resolve_pet_max_hp(foe) / denom;
    for (int i = 0; i < hits; ++i) {
        (void)g_core->deal_pink_damage(ctx, opp, grind, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(351, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_351_arm),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_351_round_end)});
