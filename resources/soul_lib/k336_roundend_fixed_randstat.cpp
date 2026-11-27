// K336 回合末固伤+随机三项族 —— effect 690（1 宏 / 1 行）
//（瀚海界王 2983 args[200,3,1]；号段 4920-4929，worktree ../agents/k336，
//  2026-10-21）
//
// 行清单（tools/soul_effects_dump.py 690，2026-10-21）：
//   「每个战斗阶段结束时附加200点固定伤害，随机使自身3项属性+1」
//   → db[0]=200 = 固伤点数（字面自证）；db[1]=3 = 随机项数（"3项"自证）；
//   db[2]=1 = 提升量（"+1"自证）——三参自洽全消费。
// ⚠️ 三查记录（a05-kprod 2026-10-21）：①内核 grep \b690\b 无魂印硬编码；②effect_info
//    690 技能空间模板不同文；③soul_lib 全 worktree/registry 无 690 注册——真缺口。
// 实现（板行处方：K302 拆原语无 heal + K267 随机互异）：
//   BATTLE_ROUND_END 桶（"战斗阶段结束"）：deal_pink_damage(对手, db[0], FIXED,
//   owner)（粉伤通道，无 heal 半边）+ 随机 db[1] 个互不相同槽 stat_change(+db[2])
//   （K267 shenjue 去重取法；自身增益通道）。对手存活门。
// 注册：SOUL_PROGRAM(690) 单节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_4920_roundend_fixed_randstat.cpp（4920-4929）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 690;

// 随机 n 个互不相同槽（K267 同款去重取法）。
int rand_distinct_slots(BattleContext& ctx, int owner, int n, int* out) {
    bool used[6] = {false, false, false, false, false, false};
    int done = 0;
    for (int i = 0; i < n && done < 6; ++i) {
        int s;
        do {
            s = choose_soul_range(ctx, kMarkId, 0, owner, owner, 0, 5);
        } while (used[s]);
        used[s] = true;
        out[done++] = s;
    }
    return done;
}

// ROUND_END：附加固伤 + 随机三项提升。
EffectResult k336_690_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(opp).hp <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    int fixed = 200;
    int count = 3;
    int gain = 1;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot >= 0 && slot <= 5) {
        const auto& db =
            ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
        if (db.size() > 0 && db[0] > 0) {
            fixed = db[0];
        }
        if (db.size() > 1 && db[1] > 0) {
            count = db[1];
        }
        if (db.size() > 2 && db[2] > 0) {
            gain = db[2];
        }
    }
    (void)g_core->deal_pink_damage(ctx, opp, fixed, DamageKind::FIXED, owner);
    int picked[6] = {};
    const int done = rand_distinct_slots(*ctx, owner, count, picked);
    for (int i = 0; i < done; ++i) {
        (void)g_core->stat_change(ctx, owner, picked[i], gain);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（690 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k336_690_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
