// K321 全 PP 压制族 —— effect 360（1 宏 / 1 行）
//（加百利 2760 args[2,1]；号段 4770-4779，worktree ../agents/k321，2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 360，2026-10-21）：
//   「每回合降低对手所有PP一点（boss免疫)」
//   → db[1]=1 = 每技能降低量（"一点"字面自证）；db[0]=2 语义不明不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b360\b 无魂印硬编码；②effect_info
//    360 技能空间模板不同文；③soul_lib 全 worktree/registry 无 360 注册——真缺口。
// 实现（板行处方：K310 直改手法全槽版）：
//   BATTLE_ROUND_START 桶（"每回合"，K2 306 同款时点）：对手全 5 技能 pp -= db[1]
//   （逐槽钳 0；pp 直改 K239/K310 手法）。boss 免疫 = 对 boss 同样生效 → 无门。
// 注册：SOUL_PROGRAM(360) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4770_pp_suppress_all.cpp（号段 4770-4779）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 360;

// ROUND_START：对手全技能 pp -db[1]（钳 0）。
EffectResult k321_360_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 ||
        !onstage_has_soul_mark(ctx, a.owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - a.owner;
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0) {
        return EffectResult::kOk;
    }
    int drain = 1;
    const int slot = ctx->find_pet_with_soulmark(a.owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[a.owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 1 && db[1] > 0) {
            drain = db[1];
        }
    }
    for (int i = 0; i < 5; ++i) {
        if (foe.skills[i].pp > 0) {
            foe.skills[i].pp = (foe.skills[i].pp > drain) ? foe.skills[i].pp - drain : 0;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（360 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k321_360_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
