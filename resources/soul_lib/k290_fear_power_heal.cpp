// K290 害怕门威力翻倍+回合回血族 —— effect 652（1 宏 / 1 行）
//（绝杀·罗格尔 2915 args[6,2,150]；号段 4460-4469，worktree ../agents/k290，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 652，2026-10-21）：
//   「对手处于害怕状态时攻击威力翻倍；每回合恢复150HP」
//   → db[0]=6 = 门异常码（害怕 Fear=6 字面自证）；db[1]=2 = 威力倍数（"翻倍"自证）；
//   db[2]=150 = 每回合回血点数（字面自证）——三参自洽全消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b652\b 无魂印硬编码；②effect_info
//    652 无模板行；③soul_lib 全 worktree/registry 无 652 注册——真缺口。
// 实现（板行处方：K9 293 条件威力门 + K277 610 ROUND_START heal）：
//   ① BATTLE_FIRST/SECOND_SKILL_EFFECT 桶（时点依据：skill_power_view 已物化，
//   k_cond_power_family 242/733 同路）：所选技为攻击技（属性技不翻）且对手处于
//   db[0] 异常（has_active_anomaly 槽查）→ ws.skill_power_view ×db[1]
//   （multiply_power_view 636/2501 同款赋值形）。
//   ② BATTLE_ROUND_START 桶：heal_amount(owner, db[2])（恢复原语，封回血钳 max）。
// 注册：SOUL_PROGRAM(652) 三节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4460_fear_power_heal.cpp（号段 4460-4469）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 652;

// 行参：db[0]=门异常码（兜底 Fear）、db[1]=倍数（兜底 2）、db[2]=回血（兜底 150）。
void k290_read_row(BattleContext* ctx, int owner, int& gate_code, int& mult,
                   int& heal) {
    gate_code = static_cast<int>(AbnormalStatusId::Fear);
    mult = 2;
    heal = 150;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        gate_code = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        mult = db[1];
    }
    if (db.size() > 2 && db[2] > 0) {
        heal = db[2];
    }
}

// ① SKILL_EFFECT：所选技攻击技 + 对手 db[0] 异常 → 威力 ×db[1]。
EffectResult k290_652_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->has_active_anomaly) {
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
    if (idx < 0 || idx >= 5 ||
        ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;
    }
    int gate_code = static_cast<int>(AbnormalStatusId::Fear);
    int mult = 2;
    int heal = 150;
    k290_read_row(ctx, owner, gate_code, mult, heal);
    if (!g_core->has_active_anomaly(ctx, opp, gate_code)) {
        return EffectResult::kOk;   // 门闭
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * mult;   // 636/2501 同款赋值形（rewritten 管线收尾重算）
    }
    return EffectResult::kOk;
}

// ② ROUND_START：每回合回血 db[2]。
EffectResult k290_652_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    int gate_code = static_cast<int>(AbnormalStatusId::Fear);
    int mult = 2;
    int heal = 150;
    k290_read_row(ctx, owner, gate_code, mult, heal);
    (void)g_core->heal_amount(ctx, owner, heal);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（652 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k290_652_skill_effect),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k290_652_skill_effect),
              SOUL_NODE(State::BATTLE_ROUND_START, &k290_652_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
