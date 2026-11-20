// K244 叠伤成长族 —— effect 842（1 宏 / 2 行）
//（阿尔斯兰 3223；author k-prod-5 2026-10-13 立单，号段 3910-3919）
//
// 行清单（tools/soul_effects_dump.py 842，2026-10-13）：
//   row558 args[30,150,1]「每次战斗阶段结束时，自身在本次战斗中的攻击伤害提高30%，
//          最高提高150%（下场时保留提升效果，BOSS无效）」
//   row566 args[35,175,1] 同上 35%/175%（阿尔斯兰战斗传承魂印进化）
//   → db[0]=每层增伤 30/35、db[1]=上限 150/175、db[2] 恒 1 无对应子句——不消费，记档。
//
// 实现（双节点）：
//   · BATTLE_ROUND_END：onstage 且存活 → 层数 +1（soulmark_storage 计数，**随宠
//     存活 = "下场时保留"**；本回合伤害吃的是上一回合层数，"每次…结束时提高"）。
//   · BATTLE_ROUND_START：onstage → ws.damage_add_pct[owner] =
//     min(db[0]×层数, db[1])（**每回合重写**套路：resetWorkspace 在 ROUND_START 桶
//     执行前（battleFsm:1680），写后整回合有效；battleContext.h:1594 AMP 加算槽）。
// boss 无效标注：tips 尾注 → 全节点 boss_invalid。
// ⚠️ 三查记录（k-prod-5 2026-10-13）：①内核无 842 魂印硬编码；②effect_info 842 同号
//    模板不同文（args_num=1 错位行，按行 tips 落地）；③soul_lib/registry 无 842 注册
//    ——真缺口。
// 先例：battle_primitives.cpp:1049/283（"ws reset→回合效果按层数重写"混地/584 套路）、
//   battleContext.h:1594（damage_add_pct 钦定注释）、k225（soulmark_storage runtime）。
// 场景回归：test/scenario/scenario_3910_stack_amp_growth.cpp（号段 3910-3919）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 842;

struct K244Runtime {
    int stacks = 0;   // 已叠层数（随宠存活 = 下场保留）
};

struct K244Row {
    int step = 30;
    int cap = 150;
};

K244Runtime& k244_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K244Runtime{};
    }
    return std::any_cast<K244Runtime&>(storage[kMarkId]);
}

K244Row k244_read_row(BattleContext* ctx, int owner) {
    K244Row row;
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    const auto& db = ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    if (db.size() > 0 && db[0] > 0) {
        row.step = db[0];
    }
    if (db.size() > 1 && db[1] > 0) {
        row.cap = db[1];
    }
    return row;
}

// ── ROUND_START：按层数重写增伤槽（resetWorkspace 之后）────────────────────────

EffectResult k244_842_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;   // 场下不供给（下场保留层数、上场继续生效）
    }
    const K244Row row = k244_read_row(ctx, owner);
    const int stacks = k244_runtime(ctx, owner).stacks;
    const int amp = row.step * stacks;
    ctx->ws.damage_add_pct[owner] += amp > row.cap ? row.cap : amp;
    return EffectResult::kOk;
}

// ── ROUND_END：叠一层 ──────────────────────────────────────────────────────────

EffectResult k244_842_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId) || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    ++k244_runtime(ctx, owner).stacks;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（842 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k244_842_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k244_842_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
