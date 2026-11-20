// D 组批产 · D14（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   圣光格劳瑞 4004 第五技 王·风雷啸世 33994 缺失效果：
//     910 当回合击败对手则令对手{0}回合内属性技能无效（切片 [2]）
//   同技能 760（不微弱）/942（消回合吸 1/3，mortarion_skills.cpp）/967（未击败附加双防
//   之和固定伤，本目录文件）均已有实现，不重复。
//
// 实现——击败门控封属（1386/1049 的 AFTER_ACTION 桥 × 884 的 seal_skill 组合）：
//   · SKILL_EFFECT 注册 FIRST/SECOND_AFTER_ACTION 双桶"只跑本回合一次"桥（582 同款）；
//   · 桥判"当回合击败对手"（对手在场宠 hp<=0；死亡漏斗在行动后之后，被击杀者仍在场）；
//   · 击败 → seal_skill(attribute=true, attack=false, count=0, duration_rounds={0},
//     hit_invalid=false)：count=0 = 窗口内不限次数（"N回合内属性技能无效"是状态而非
//     次数）；hit_invalid=false = 完全无效（SKILL_INVALID 补偿语义，区别于 781/1922 的
//     命中失效）；回合票在 ROUND_REDUCTION 递减，授予回合尾算第一扣。

#include "common.h"

#include <memory>
#include <vector>

namespace {

// ── 910 当回合击败对手 → 对手 {n} 回合内属性技能无效 ──────────────────────────
EffectResult effect_910_kill_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp > 0) {
        return EffectResult::kOk;   // 当回合未击败 → 不封
    }
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/910,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

EffectResult effect_910_seal_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // AFTER_ACTION 桥：FIRST/SECOND 各一条，只有行动方那条跑（582/1049 同款双注册）。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(910, 0, self, 1, EffectArgs(std::vector<int>{self, opp, rounds}),
                 &effect_910_kill_bridge);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 910;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(910, effect_910_seal_on_kill);
