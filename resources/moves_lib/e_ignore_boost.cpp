// E11 · 无视强化族 —— 195×75 / 494×65 / 486×65（2026-09-29，worktree ../agents/e11）
//
// 家族语义："无视对手能力提升状态"——攻击命中时防御方的正等级视作 0 级
//（弱化保留：官方字面只无视"提升"）。
//
// 内核通道（本次随本文件落地，用户定稿的**视图方案**）：
//   ws.boost_pierced[defender]（视图旗标）→ 伤害公式防御/特防取值点
//   （Calculation::calculateDamage → getTempAbilityValue 的 pierce_boost 参数）
//   按 min(0, level) 取有效等级 → handle_Battle*AfterActionEnd 行动末清
//  （"本次命中"语义：SKILL_EFFECT 写 → ATTACK_DAMAGE 消费 → 行动末清；
//   连击多段在同一行动内全生效，不跨行动泄漏——defense_ignore_pct 套装线的
//   同位先例，但那是恒定条款不清、本族每次命中要清）。
//
// 195（args_num=0）"无视对手**双防**能力提升状态"：双防=伤害公式唯一消费面
//   （def/spdef），与 494 在伤害口径上同槽 → 与 494 共用同一 handler
//  （旗标只作用于伤害取值点，速度比较/命中不受影响）。
//   实例：神兵器肯扎特 最终协议·耀变 37883（[195,1922,1233,1122]）、
//         钢傀 熔甲轰击 38413 / 迅灭光雨 38412。
// 494（args_num=0）"无视对手能力提升状态"：全槽字面，伤害口径同 195 → 共 handler。
//   实例：工程型TOH TH地基摇撼 38491、西曼德妮 变量清除 38432、怒雷神裁·雷伊 瞬影天雷斩 37917。
// 486（args_num=1）"下{0}回合若自身选择使用技能则无视对手能力提升状态"：
//   NextRounds 双桶窗口（BEFORE_SKILL_HIT × FIRST/SECOND，470 模式）——
//   每次出手 tick 写旗标（先于同行动的 ATTACK_DAMAGE 消费），行动末随内核清。
//   实例：萨克斯特 邪灵怨念 27505（"下2回合自身所有攻击将无视对手能力强化状态"）。
//
// 前置三查（2026-09-29）：①内核原无 195/494/486 处理（真缺口，非 697 式假阳性）
// ②effect_meta 无穿透分类 ③全库无既有实现。
//
// 场景回归：test/scenario/scenario_570_ignore_boost.cpp。

#include "common.h"

namespace {

// ── 195 / 494 —— 本次命中无视对手能力提升（共 handler：伤害口径同槽）──
EffectResult effect_ignore_boost_this_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // SKILL_EFFECT 桶（HIT 分支）在 ATTACK_DAMAGE 之前执行 → 旗标正好覆盖本次
    // 命中的全部伤害段（连击多段），行动末由 FSM 清。
    ctx->ws.boost_pierced[target] = true;
    return EffectResult::kOk;
}

// ── 486 窗口 tick —— 窗口期内每次出手（BEFORE_SKILL_HIT，命中与否都算"使用"）
//      写旗标；同行动的 ATTACK_DAMAGE 消费 ──
EffectResult effect_486_pierce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.boost_pierced[target] = true;
    return EffectResult::kOk;
}

// ── 486 —— 下{0}回合若自身使用技能则无视对手能力提升状态 ──
EffectResult effect_486_ignore_boost_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // "下{0}回合"= NextRounds 起点（本回合不算）；双桶（先/后手出手都写）。
    // tick 在 BEFORE_SKILL_HIT 时点（先于同行动 ATTACK_DAMAGE），行动末随内核清——
    // 窗口 × 每次出手重新写，语义正好是"窗口期内每次出手都无视"。
    for (State st : {State::BATTLE_FIRST_BEFORE_SKILL_HIT, State::BATTLE_SECOND_BEFORE_SKILL_HIT}) {
        register_next_rounds_effect(ctx, st, owner, 486, rounds, {owner, target},
                                    &effect_486_pierce_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

// 195/494 共享实现体（SKILL_EFFECT 注册变量按函数名生成——薄壳规避重定义）
EffectResult effect_195_ignore_boost_this_hit(BattleContext* ctx, const EffectArgs& args) {
    return effect_ignore_boost_this_hit(ctx, args);
}
EffectResult effect_494_ignore_boost_this_hit(BattleContext* ctx, const EffectArgs& args) {
    return effect_ignore_boost_this_hit(ctx, args);
}

SKILL_EFFECT(195, effect_195_ignore_boost_this_hit)
SKILL_EFFECT(494, effect_494_ignore_boost_this_hit)
SKILL_EFFECT(486, effect_486_ignore_boost_window)
