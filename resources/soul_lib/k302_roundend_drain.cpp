// K302 回合末吸取族 —— effect 735（1 宏 / 1 行）
//（风玄灵尊 3050 args[150,0]；号段 4580-4589，worktree ../agents/k302，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 735，2026-10-21）：
//   「每个战斗阶段结束时{吸取}对手150点{体力}」
//   → db[0]=150 = 吸取点数（字面自证）；db[1]=0 废位不消费（K1 纪律）。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b735\b 无魂印硬编码；②effect_info
//    735 无模板行；③soul_lib 全 worktree/registry 无 735 注册——真缺口。
// 实现（板行处方：绝对值吸取 = deal_pink_damage(FIXED) + heal_amount 等量——drain_hp
//   是分数版（max/denom），本行绝对值故拆两原语）：BATTLE_ROUND_END 桶（"每个战斗
//   阶段结束时"，1011/K178 定例时点）：dmg = min(db[0], 对手当前血)（吸取取不了比
//   血多的，口径记档）→ deal_pink_damage(对手, dmg, FIXED, owner)（粉伤通道，吃护罩/
//   免粉/减伤）→ heal_amount(owner, dmg)（按预取量等量恢复；粉伤被减免时恢复不缩，
//   口径记档）。粉伤不自动过死亡漏斗 → 对手血尽不显式判死（无"击杀"归因语义，与
//   盖亚反弹的差异记档）。
// 注册：SOUL_PROGRAM(735) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4580_roundend_drain.cpp（号段 4580-4589）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 735;

// ROUND_END：吸取对手 min(db[0], 对手血) 点，自身等量恢复。
EffectResult k302_735_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    ElfPet& foe = ctx->getPet(opp);
    if (foe.hp <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    int points = 150;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            points = db[0];
        }
    }
    const int dmg = (points > foe.hp) ? foe.hp : points;   // 吸取不越过对手存量
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, dmg);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（735 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k302_735_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
