// K297 强化态回血族 —— effect 249（1 宏 / 1 行）
//（库拉里尔 3151 args[1,4,1,1]；号段 4530-4539，worktree ../agents/k297，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 249，2026-10-21）：
//   「自身处于能力强化状态时，所有技能使自身恢复最大体力的1/4（boss无效）」
//   → db[1]=4 = 回血分母（"1/4"字面自证）；db[0]/db[2]/db[3] 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b249\b 无魂印硬编码（圣莫 2492
//    文件为同号异实技能效果）；②effect_info 249 无模板行；③soul_lib 全
//    worktree/registry 无 249 注册——真缺口。
// 实现（板行处方：FIRST/SECOND_SKILL_EFFECT 强化态门 + heal rider）：
//   所选技任意（"所有技能"——攻击/属性技均触发，miss 也算）→ 自身任一能力槽 >0
//   （"处于能力强化状态"，6 槽全扫，K283 659 同读）→ heal_amount(owner, max/db[1])
//   （恢复原语，封回血覆盖则不涨，上限钳 max）。boss 无效 → 节点 boss_invalid 旗。
// 注册：SOUL_PROGRAM(249) 双节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4530_raised_heal_quarter.cpp（号段 4530-4539）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 249;

// SKILL_EFFECT：强化态门 → 回血 max/db[1]（所有技能）。
EffectResult k297_249_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    bool raised = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount && !raised; ++i) {
        raised = ctx->ability_levels[owner][i] > 0;
    }
    if (!raised) {
        return EffectResult::kOk;   // 非强化态
    }
    int denom = 4;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 1 && db[1] > 0) {
            denom = db[1];
        }
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, max_hp / denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（249 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k297_249_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k297_249_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
