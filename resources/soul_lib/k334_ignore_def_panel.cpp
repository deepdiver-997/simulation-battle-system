// K334 无视防御比例族 —— effect 668（1 宏 / 1 行）
//（超时空暴龙 2945 args[20,1]；号段 4900-4909，worktree ../agents/k334，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 668，2026-10-21）：
//   「无视对手20%的防御属性;(BOSS无效)」
//   → db[0]=20 = 无视比例%（字面自证）；db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b668\b 无魂印硬编码；②effect_info
//    668 无模板行；③soul_lib 全 worktree/registry 无 668 注册——真缺口。
// 实现（板行处方：k_panel_family numericalBase 乘区落法）：
//   OPERATION_ENTER_EXIT_STAGE 节点（own 入场，early）：对**当前对手**的防御面板
//   （numericalBase[DEF]）一次性 ×(100-db[0])/100——"无视"以对手面板削峰落地
//   （口径记档：面板写对全体攻击者生效，非仅印主；逐宠去重防重复乘区——
//   plugin_storage 键 = 基座+对手侧*8+槽位）。boss 无效 → 节点旗。
// 注册：SOUL_PROGRAM(668) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4900_ignore_def_panel.cpp（号段 4900-4909）。

#include "common.h"

namespace {

constexpr int kMarkId = 668;
constexpr int kAppliedKeyBase = 990834000;   // +opp*8+slot：该宠已削防去重

// 入场：当前对手防御面板一次性 ×(100-db[0])%。
EffectResult k334_668_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const int foe_slot = ctx->on_stage[opp];
    if (foe_slot < 0 || foe_slot > 5) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const int dedup_key = kAppliedKeyBase + opp * 8 + foe_slot;
    if (ctx->plugin_storage.count(dedup_key)) {
        return EffectResult::kOk;   // 该对手宠已削过（一次性）
    }
    int pct = 20;
    const int my_slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (my_slot >= 0 && my_slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[my_slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            pct = db[0];
        }
    }
    ctx->plugin_storage.emplace(dedup_key, 1);
    const auto idx = NumericalPropertyIndex::DEFENSE;
    const int after = foe.numericalBase[idx] * (100 - pct) / 100;
    foe.numericalBase[idx] = after;        // 权威基线
    foe.numericalProperties[idx] = after;  // 工作副本同步（k_panel suit④范式）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（668 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k334_668_enter,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
