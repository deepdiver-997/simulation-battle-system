// K332 增长吸取族 —— effect 688（1 宏 / 1 行）
//（仙化灵龙 2974 args[9,9,3,1]；号段 4880-4889，worktree ../agents/k332，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 688，2026-10-21）：
//   「第一战斗阶段结束时吸收对手1/9最大体力,每回合增加1/9的吸收量, 最多吸收1/3最大体力(boss无效)」
//   → db[0]=9 = 分母（"1/9"自证）；db[1]=9 = 步长分母（"增加1/9"自证，步长 =
//   db[0]/db[1] = 1）；db[2]=3 = 上限分子（"1/3" = 3/9 自证）；db[3]=1 废位。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b688\b 无魂印硬编码；②effect_info
//    688 技能空间模板不同文；③soul_lib 全 worktree/registry 无 688 注册——真缺口。
// 实现（板行处方：K302 拆原语 + K296 账本拼装）：
//   BATTLE_ROUND_END 桶（"战斗阶段结束"）：分子账本（plugin_storage）首挂 db[0]/
//   db[1]（首回合吸收 1/9），每回合 += db[0]/db[1]，钳 db[2]（上限分子）；
//   dmg = 对手 max × 分子 / db[0]（吸取不越过对手存量，K302 口径）→
//   deal_pink_damage(FIXED) + heal_amount 等量。boss 无效 → 节点旗。
// 注册：SOUL_PROGRAM(688) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4880_growing_absorb.cpp（号段 4880-4889）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 688;
constexpr int kNumKeyBase = 990832000;   // +owner：分子账本（几宫格 1/db[0]）

// ROUND_END：账本增长 → 吸取 max×分子/db[0] + 等量恢复。
EffectResult k332_688_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
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
    int denom = 9;
    int step_denom = 9;
    int cap_num = 3;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            denom = db[0];
        }
        if (db.size() > 1 && db[1] > 0) {
            step_denom = db[1];
        }
        if (db.size() > 2 && db[2] > 0) {
            cap_num = db[2];
        }
    }
    const int step = (step_denom > 0) ? denom / step_denom : 1;   // 9/9 = 1 宫格
    if (step <= 0) {
        return EffectResult::kOk;
    }
    const int key = kNumKeyBase + owner;
    int num = any_int_or(ctx->plugin_storage[key], 0);
    num = (num + step > cap_num) ? cap_num : num + step;
    ctx->plugin_storage[key] = num;
    const int foe_max = resolve_pet_max_hp(foe);
    int dmg = foe_max * num / denom;
    if (dmg > foe.hp) {
        dmg = foe.hp;   // 吸取不越过对手存量（K302 口径）
    }
    if (dmg <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, dmg);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（688 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k332_688_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
