// K237 周期吸取族 —— effect 665（1 宏 / 1 行）
//（奥德迪尔 2929；author k-prod-6 2026-10-08 立单，号段 3810-3819）
//
// 行清单（tools/soul_effects_dump.py 665，2026-10-08）：
//   row339 args[2,3]（kind[5]）「每2回合吸收对手1/3最大HP(boss无效)」
//   → db[0]=周期（2 回合）、db[1]=吸取分母（1/3）。
//
// 实现：
//   BATTLE_ROUND_START 桶：onstage 且 roundCount % db[0] == 0（rc 从 0 起——R1/R3/R5…
//   每 db[0] 回合首；roundCount 本身即周期账本，无需另立 K7 计数器——口径记档）
//   且双方在场宠存活 → 吸取双段（k225 341/yanshou 同款）：
//   deal_pink_damage(opp, max_hp(opp)/db[1], PERCENT_VALUE, own) + heal_amount(own, 同值
//   ——回复按 heal_amount 封顶，粉伤全额不变；满体时"吸收"只出伤不回血，k225 同口径）。
// ⚠️ 三查记录（k-prod-5 2026-10-08）：①内核 skills.cpp:123 case 665 为**技能空间**
//    同号硬编码（effect_info 665"伤害低于{0}则受伤减少"），与魂印 665 属两个注册空间
//    （常识 §二 双空间口径）——魂印行为无内核硬编码；②effect_info 665 模板不同文
//    （错位行，按行 tips 落地）；③soul_lib/registry 无 665 注册——真缺口。
// 先例：k225 341（吸取双段值形）、k209/k225（ROUND 完成点账本节点形态）。
// 场景回归：test/scenario/scenario_3810_periodic_drain.cpp（号段 3810-3819）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 665;

struct K237Row {
    int period = 2;
    int drain_denom = 3;
};

K237Row k237_read_row(BattleContext* ctx, int owner) {
    K237Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.period = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        row.drain_denom = db[1];
    }
    return row;
}

// ── 周期吸取（ROUND_START，rc%period==0 触发）────────────────────────────────

EffectResult k237_665_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K237Row row = k237_read_row(ctx, owner);
    if (row.period <= 0 || ctx->roundCount % row.period != 0) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(opp);
    if (self.hp <= 0 || foe.hp <= 0) {
        return EffectResult::kOk;
    }
    const int drain = resolve_pet_max_hp(foe) / row.drain_denom;
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, drain, DamageKind::PERCENT_VALUE, owner);
    (void)g_core->heal_amount(ctx, owner, drain);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（665 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k237_665_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
