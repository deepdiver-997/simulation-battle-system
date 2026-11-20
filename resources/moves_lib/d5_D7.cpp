// D 组批产 · D7（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   杰诺 3408 第五技 击穿次元壁 30961 缺失效果（4399 原文三句对齐）：
//     781  消除对手回合类效果，消除成功则{0}回合内令对手使用的属性技能无效
//          —— 已由 selection_seal.cpp 实现，不重复。
//     1091 未击败对手则自身下{0}回合免疫并反弹异常状态（切片 [2]）
//     1049 击败对手则自身全属性+{0}（切片 [1]）
//
// 实现——「击败/未击败」族，与同技能 582（未击败→下N回合必暴，effect_batch8.cpp）
// **同判据同桥式**，保证三句互补不双发：
//   · 桥：SKILL_EFFECT 时点在 FIRST/SECOND_AFTER_ACTION 双桶各注册一条"只跑本回合
//     一次"的节点（register_once_this_round 复刻件，锚 ctx->roundCount 过期——
//     不能走 round_effect_start_round，否则后手方桥下一回合再跑一次）。
//     ①层命中门控结构性生效：miss/被封时 SKILL_EFFECT 整桶不跑 → 两句都不触发。
//   · 击败判据 = AFTER_ACTION 时点对手在场宠 hp<=0（582 同款；死亡漏斗在行动后
//     时点之后，被击杀者此刻仍在场 hp=0）。
//   · 1049 击败 → all_stats_up 立即生效（66"击败吸血"同款探针直发）。
//   · 1091 未击败 → 下一回合 ROUND_START 一次性授予 191 同款双票（grant_immunity
//     Modern ANOMALY + grant_reflect），rounds={0} 从下回合起算 = 恰好"下{0}回合"
//     （当回合尾不覆盖；直接在桥上授票会多覆盖当回合，且后出手方被回合扣减吃掉一档）。

#include "common.h"

#include <memory>
#include <vector>

namespace {

// ── 桥接节点：只在当前回合跑一次（effect_batch8 同款，本地一份）────────────────
inline void register_once_this_round(BattleContext* ctx, State trigger, int owner,
                                     int effect_id, std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || !fn) {
        return;
    }
    Effect e(effect_id, 0, owner, /*rounds=*/1, EffectArgs(std::move(ints)), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, /*duration=*/1,
                                                 ctx->roundCount);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
}

inline void register_after_action_bridge(BattleContext* ctx, int owner, int effect_id,
                                         std::vector<int> ints, EffectFn fn) {
    // FIRST/SECOND 各注册一条，只有行动方那条会跑 → 恰好一次（582/532 同款双注册）。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_once_this_round(ctx, st, owner, effect_id, ints, fn);
    }
}

// ── 1049 击败对手则自身全属性+{0}；桥内 Args=[self, opp, up] ───────────────────
EffectResult effect_1049_kill_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int up = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || up <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp > 0) {
        return EffectResult::kOk;   // 未击败 → 本条不给（582 同判据取反）
    }
    all_stats_up(ctx, self, up);
    return EffectResult::kOk;
}

EffectResult effect_1049_kill_all_stats_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int up = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || up <= 0) {
        return EffectResult::kOk;
    }
    register_after_action_bridge(ctx, self, 1049, {self, opp, up},
                                 &effect_1049_kill_bridge);
    return EffectResult::kOk;
}

// ── 1091 未击败对手则自身下{0}回合免疫并反弹异常 ────────────────────────────────
// 第二跳：下回合 ROUND_START 授 191 同款双票（immunity Modern + reflect）。
EffectResult effect_1091_grant_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/1091, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1091,
                        /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(self, /*source_slot=*/-1, /*source_effect_id=*/1091,
                                    /*target=*/self, /*rounds=*/rounds);
    return EffectResult::kOk;
}

EffectResult effect_1091_undeathed_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 本条不给（582 同判据）
    }
    // 下回合 ROUND_START 一次性授予（NextRounds 窗口 rounds=1 = 恰好下一回合跑一次）。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1091,
                                /*rounds=*/1, {self, opp, rounds},
                                &effect_1091_grant_tick);
    return EffectResult::kOk;
}

EffectResult effect_1091_undeathed_anomaly_reflect(BattleContext* ctx,
                                                   const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_after_action_bridge(ctx, self, 1091, {self, opp, rounds},
                                 &effect_1091_undeathed_bridge);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1049, effect_1049_kill_all_stats_up);
SKILL_EFFECT(1091, effect_1091_undeathed_anomaly_reflect);
