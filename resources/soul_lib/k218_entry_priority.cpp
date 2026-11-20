// K218 首回合先制族 —— effect 277（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 277，2026-10-07）：
//   277 神威·马超 2500 args[]（零参）：「登场首回合所有攻击先制+1,并且增加10%致命
//     一击概率」→ 子句②暴击率+10%：内核无暴击率视图通道，**记档不做**（K1 纪律，
//     立单行已注明）。
//
// ⚠️ 实现形态（T4 类型化行参）：双节点——
//   ①enter 节点：记登场回合（soulmark_storage{entry_round}，每次登场刷新——"登场
//     首回合"按每次入场语义；开局 early 双触发同值幂等）。
//   ②BATTLE_FIRST_MOVE_RIGHT 桶节点（yanshou 1123/636 同款：ws.preemptive_level 在
//     MOVE_RIGHT memset 之后写）——onstage 且 roundCount == 登场回合 →
//     ws.preemptive_level[owner] += 1（当回合所有攻击先制+1）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 277 魂印行为硬编码 ②effect_info
//    无 277 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：yanshou_4875（BATTLE_FIRST_MOVE_RIGHT 写 preemptive_level 全形态）、
//   k218 立单行 K 判据（SOP §二.2）。
// 场景回归：test/scenario/scenario_3610_entry_priority.cpp（号段 3610-3619）。

#include "common.h"

namespace {

constexpr int kMarkId = 277;

struct K218Runtime {
    int entry_round = -1;
};

K218Runtime& k218_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K218Runtime{};
    }
    return std::any_cast<K218Runtime&>(storage[kMarkId]);
}

// ① enter：记登场回合。
EffectResult k218_277_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    K218Runtime& rt = k218_runtime(ctx, a.owner);
    rt.entry_round = ctx->roundCount;
    return EffectResult::kOk;
}

// ② FMR 桶：登场首回合 → 所有攻击先制 +1。
EffectResult k218_277_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K218Runtime& rt = k218_runtime(ctx, owner);
    if (rt.entry_round != ctx->roundCount) {
        return EffectResult::kOk;   // 仅登场首回合
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（277 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k218_277_enter),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k218_277_move_right)});
