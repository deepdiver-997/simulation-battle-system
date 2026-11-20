// K222 对手等级变化回血族 —— effect 338（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 338，2026-10-07）：
//   338 司马懿 2696 args[2, 6]：「若对手本回合能力等级发生变化，则战斗阶段结束时恢复
//     1/6最大HP」→ args 语义存疑不消费（记档）；1/6 由 tips 承载。
//
// ⚠️ 实现形态（T4 类型化行参）：双节点（STAGE）——
//   ①ROUND_START：快照对手 6 槽等级（soulmark_storage 账本{回合号, slots[6]}）。
//   ②ROUND_END：回合号匹配且任一槽 ≠ 快照 → heal_amount(resolve_pet_max_hp/6)
//     （K163 261 同槽位回血）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 338 魂印行为硬编码 ②effect_info
//    无 338 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k_open_window_immune 261（ROUND_START 回血）、K206（条件回血门）。
// 场景回归：test/scenario/scenario_3640_level_change_heal.cpp（号段 3640-3649）。

#include "common.h"

#include <array>

namespace {

constexpr int kMarkId = 338;

struct K222Runtime {
    int snapshot_round = -1;
    std::array<int, BattleContext::kAbilityLevelSlotCount> slots{};
};

K222Runtime& k222_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K222Runtime{};
    }
    return std::any_cast<K222Runtime&>(storage[kMarkId]);
}

// ① ROUND_START：快照对手 6 槽。
EffectResult k222_338_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    K222Runtime& rt = k222_runtime(ctx, owner);
    rt.snapshot_round = ctx->roundCount;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        rt.slots[i] = ctx->ability_levels[opp][i];
    }
    return EffectResult::kOk;
}

// ② ROUND_END：对手本回合等级有变化 → 回 1/6 最大 HP。
EffectResult k222_338_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K222Runtime& rt = k222_runtime(ctx, owner);
    if (rt.snapshot_round != ctx->roundCount) {
        return EffectResult::kOk;   // 无本回合快照
    }
    const int opp = ctx->opponent(owner);
    bool changed = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount && !changed; ++i) {
        changed = rt.slots[i] != ctx->ability_levels[opp][i];
    }
    if (!changed) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    (void)g_core->heal_amount(ctx, owner, resolve_pet_max_hp(self) / 6);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（338 入账，只增不改）────────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::BATTLE_ROUND_START, &k222_338_round_start),
                       SOUL_NODE(State::BATTLE_ROUND_END, &k222_338_round_end)});
