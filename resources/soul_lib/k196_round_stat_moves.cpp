// K196 每回合能力升降族 —— effect 390（1 宏 5 行）
//
// 行清单（tools/soul_effects_dump.py 390，2026-10-07）：
//   390：2815 煌怒·天煞 [1,1,1,2,1,2,1]「每回合特攻+1、速度+1」/
//       2823 耀影·绝煞 [1,1,2,1,2,1,1]「每回合双防+1」/
//       2914 剑神·鲁伊斯 [1,2,1,1,1,2,1]「每回合攻击+1、速度+1」/
//       3064 末日天卫 [1,1,3,1,3,1,1]「每回合双防+2」/
//       3113 梦魔·狄修卡 [2,1,1,1,1]「每回合使对手速度-1，命中-1」
// 实现：ROUND_START 声明节点（K4 641/622 同族）+ onstage 门 → monster_table 行文驱动
// （K5 7「args 空以行文为数」同款先例；⚠️ 五行 args 凑不出一致映射——K4 当年记档
// 即因此，本单不消费 args、布局原样记档待未来解码）；self 正向走 stat_change
// （本体+视图同步，不查免弱=自身增益通道），对手侧负向走 stat_drop
// （附加禁令/免弱/钳制原语内查）。每回合各一跳，上下限原语内钳。
// ⚠️ 三查记录（2026-10-07）：①内核无 390 硬编码；②soul_lib 并集无注册
// （K4 当年 6 宏落地 3 项、390 记档遗留，本单补齐）；③五行同构互锚、4399✓。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

struct StatMove {
    int target_offset;   // 0=自身 1=对手
    int slot;            // 等级槽 0攻 1特攻 2防 3特防 4速 5命中
    int delta;           // 正=提升（stat_change）负=弱化（stat_drop）
};

struct MonsterMoves {
    int monster_id;
    StatMove moves[2];
};

constexpr MonsterMoves kTable[] = {
    {2815, {{0, 1, +1}, {0, 4, +1}}},   // 煌怒·天煞：特攻+1 速+1
    {2823, {{0, 2, +1}, {0, 3, +1}}},   // 耀影·绝煞：双防+1
    {2914, {{0, 0, +1}, {0, 4, +1}}},   // 剑神·鲁伊斯：攻+1 速+1
    {3064, {{0, 2, +2}, {0, 3, +2}}},   // 末日天卫：双防+2
    {3113, {{1, 4, -1}, {1, 5, -1}}},   // 梦魔·狄修卡：对手速-1 命中-1
};

// ── effect 390：每回合自动升降能力项 ───────────────────────────────────────

EffectResult k196_390_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 390)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, 390);
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const int monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    for (const auto& entry : kTable) {
        if (entry.monster_id != monster_id) {
            continue;
        }
        for (const auto& move : entry.moves) {
            if (move.delta == 0) {
                continue;
            }
            if (move.delta > 0) {
                g_core->stat_change(ctx, owner, move.slot, move.delta);
            } else {
                g_core->stat_drop(ctx, ctx->opponent(owner), move.slot, -move.delta, owner);
            }
        }
        break;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(390, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &k196_390_round_start)});
