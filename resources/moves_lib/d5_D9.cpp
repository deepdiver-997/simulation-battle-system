// D9 · 不灭·艾恩斯 3467 第五技「王·焚世烈焰 31273」缺失效果 777
//（夜跑 night-2，2026-09-28）
//
// 31273 链 = [760, 777, 1048, 1257]、args [1, 75, 3]：
//   760=[]（攻击不微弱，已接两处）、777=[1]（本文件）、
//   1048=[75]（对手异常增伤，effect_batch8.cpp 已接）、1257=[3]（非本单）。
// 4399 图鉴（王·焚世烈焰）："必中；攻击时造成的伤害不会出现微弱；消除对手能力上升
//   状态，消除成功下1回合必定先出手；对手处于异常时造成的伤害提高75%；
//   对手不处于异常状态则吸取对手最大体力1/3"。
//
// 777 "消除对手能力上升状态，消除成功下{0}回合必定先出手"：
//   - 消除 = clear_stat_boosts 原语，返回值 >0 才算成功（1217/消强必先桩同判）；
//   - "下{0}回合必定先出手"= NextRounds 窗口节点（register_next_rounds_effect，本回合
//     不算、从下回合起算），每回合 MOVE_RIGHT 时点写 ws.guaranteed_first=tier 1——
//     该字段在先手权结算开头 memset、由回合窗口节点重写（694 同通道；必先比先制
//     等级更强，tier 越高越先，普通必先取 1 与"消强必先"桩一致）。
//
// 场景回归：test/scenario/scenario_250_aiensi_3467_777.cpp。

#include "common.h"

namespace {

// ── 777 —— 消除对手能力上升状态，消除成功下{0}回合必定先出手 ──
EffectResult effect_777_clear_then_guaranteed_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 无上升可清 → 消除失败，无后续
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 777, rounds,
                                {owner, target, /*tier=*/1}, &effect_set_guaranteed_first);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(777, effect_777_clear_then_guaranteed_first);
