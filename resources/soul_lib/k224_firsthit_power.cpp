// K224 首回合必中增伤族 —— effect 257（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 257，2026-10-07）：
//   257 琳娜 2389 / 赛琳娜 2390 args[]（零参）：「出战首回合所有技能必中,出战首回合
//     攻击技能造成的伤害提升15%」
//
// ⚠️ 实现形态（T4 类型化行参）：双节点（K218 直系变体）——
//   ①FMR 桶：登场首回合门（soulmark_storage{entry_round}）→
//     ws.must_hit_grant[owner] += 1（memset 后写，CLAUDE 判据；必中=跳过命中率掷骰）。
//   ②SKILL_EFFECT view（K187 直系）：登场首回合门 + roundChoice/类型表门 →
//     view×115/100。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 257 魂印行为硬编码 ②effect_info
//    无 257 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k218_entry_priority（FMR + entry_round 账本全形态）、k187_shield_power（view）。
// 场景回归：test/scenario/scenario_3680_firsthit_power.cpp（号段 3680-3689）。

#include "common.h"

namespace {

constexpr int kMarkId = 257;

struct K224Runtime {
    int entry_round = -1;
};

K224Runtime& k224_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K224Runtime{};
    }
    return std::any_cast<K224Runtime&>(storage[kMarkId]);
}

EffectResult k224_257_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    k224_runtime(ctx, a.owner).entry_round = ctx->roundCount;
    return EffectResult::kOk;
}

// ① FMR 桶：登场首回合 → 必中授予。
EffectResult k224_257_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K224Runtime& rt = k224_runtime(ctx, owner);
    if (rt.entry_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    ctx->ws.must_hit_grant[owner] += 1;
    return EffectResult::kOk;
}

// ② SKILL_EFFECT 桶：登场首回合攻击技 → 威力 +15%。
EffectResult k224_257_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5
        || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;
    }
    const K224Runtime& rt = k224_runtime(ctx, owner);
    if (rt.entry_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * 115 / 100;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（257 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k224_257_enter),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k224_257_move_right),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k224_257_skill_effect),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k224_257_skill_effect)});
