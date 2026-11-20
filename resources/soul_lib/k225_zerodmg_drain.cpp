// K225 零伤吸取族 —— effect 341（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 341，2026-10-07）：
//   341 伊泽雷尔 2702 args[30, 0]：「若本回合自身造成的攻击伤害为0，则本战斗阶段结束
//     时{吸取}对手最大体力的1/3」→ args 语义存疑不消费（记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：双节点（K209 直系变体）——
//   ①TAKE_DAMAGE watcher（arm-once）：ev.target==opp 且 ev.actor==owner 且 amount>0
//     → 记"本回合已造成攻击伤害"账本（回合号变更即清）。
//   ②RC 节点：账本==0（本回合攻击零伤/未攻击）→ deal_pink_damage(opp, max/3,
//     PERCENT_VALUE) + heal_amount(同值)（yanshou 吸取双段同款）。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 341 魂印行为硬编码 ②effect_info
//    无 341 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k209_damage_to_heal（账本+RC 双节点全形态）、yanshou（吸取双段）。
// 场景回归：test/scenario/scenario_3700_zerodmg_drain.cpp（号段 3700-3709）。

#include "common.h"

namespace {

constexpr int kMarkId = 341;

struct K225Runtime {
    bool watcher_armed = false;
    int ledger_round = -1;
    int dealt = 0;
};

K225Runtime& k225_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K225Runtime{};
    }
    return std::any_cast<K225Runtime&>(storage[kMarkId]);
}

EffectResult k225_341_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K225Runtime& rt = k225_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const int opp = ctx->opponent(owner);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp](BattleContext* wctx, const BattleEvent& ev) {
            if (ev.target != opp || ev.actor != owner || ev.amount <= 0) {
                return;   // 仅 own 攻击对 opp 的实际伤害入账
            }
            K225Runtime& rt = k225_runtime(wctx, owner);
            if (rt.ledger_round != wctx->roundCount) {
                rt.ledger_round = wctx->roundCount;
                rt.dealt = 0;
            }
            rt.dealt += ev.amount;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult k225_341_round_completion(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    K225Runtime& rt = k225_runtime(ctx, owner);
    const bool damaged_this_round =
        rt.ledger_round == ctx->roundCount && rt.dealt > 0;
    rt.ledger_round = -1;
    rt.dealt = 0;
    if (damaged_this_round) {
        return EffectResult::kOk;   // 门：本回合攻击伤害为 0（含未攻击出伤）
    }
    const int opp = ctx->opponent(owner);
    ElfPet& foe = ctx->getPet(opp);
    const int drain = resolve_pet_max_hp(foe) / 3;
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->deal_pink_damage(ctx, opp, drain, DamageKind::PERCENT_VALUE, owner);
    (void)g_core->heal_amount(ctx, owner, drain);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（341 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k225_341_arm,
                        /*once=*/false, /*early=*/true),
              SOUL_NODE(State::BATTLE_ROUND_COMPLETION,
                        &k225_341_round_completion)});
