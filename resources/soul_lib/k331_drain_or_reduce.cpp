// K331 双分支吸取/减伤族 —— effect 683（1 宏 / 1 行）
//（生灵之主·蒙娜丽莎 2966 args[4,1,30]；号段 4870-4879，worktree ../agents/k331，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 683，2026-10-21）：
//   「体力低于对手时每回合{吸取}对手1/4{体力}（boss无效）,体力高于对手时受到的伤害减少30%」
//   → db[0]=4 = 吸取分母（"1/4"字面自证）；db[2]=30 = 减伤%（"30%"字面自证）；
//   db[1]=1 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b683\b 无魂印硬编码；②effect_info
//    683 技能空间模板不同文；③soul_lib 全 worktree/registry 无 683 注册——真缺口。
// 实现（板行处方：ROUND_START hp 比较双分支）：
//   BATTLE_ROUND_START 桶（"每回合"）：自身存活且对手存活 →
//   · 自身 hp < 对手 hp → drain_hp(owner, 对手, db[0])（分数吸取原语：目标掉
//     max/db[0]、自己等量恢复——K287 同原语）；
//   · 自身 hp > 对手 hp → ws.damage_reduce_add[owner] 首空槽推 db[2]（diwang 传承
//     槽加算减伤，ws 每回合 reset → 每回合重写）。
//   相等不落分支。boss 无效 → 节点 boss_invalid 旗。
// 注册：SOUL_PROGRAM(683) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4870_drain_or_reduce.cpp（号段 4870-4879）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 683;

// ROUND_START：hp 比较双分支。
EffectResult k331_683_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& foe = ctx->getPet(opp);
    if (self.hp <= 0 || foe.hp <= 0) {
        return EffectResult::kOk;
    }
    int denom = 4;
    int reduce = 30;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            denom = db[0];
        }
        if (db.size() > 2 && db[2] > 0) {
            reduce = db[2];
        }
    }
    if (self.hp < foe.hp) {
        (void)g_core->drain_hp(ctx, owner, opp, denom);
        return EffectResult::kOk;
    }
    if (self.hp > foe.hp) {
        int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
        for (int i = 0; i < 4; ++i) {
            if (slots[i] == 0) {
                slots[i] = reduce;
                break;
            }
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（683 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k331_683_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
