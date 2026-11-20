// J66 · 反转先制族 —— 743×16 / 1491×17（2026-10-03，worktree ../agents/j66）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 743（args_num=2）"反转对手能力提升，反转成功下{0}回合先制+{1}"：
//   - 反转 = stat_boost_reversal 原语（1110 同款：第一件事查免弱 STAT_DROP；
//     REVERSED=有正槽被压成下降 / NOTHING=本无提升 / BLOCKED=免弱）；
//   - "反转成功" = REVERSED → NextRounds({0}) 的 MOVE_RIGHT 先制窗
//     （J61 1726 载荷 effect_move_right_priority_bonus 直系：{owner,X,+{1}}）；
//     NOTHING/BLOCKED 都不算成功、不武装窗（对照取严）。
//   - 实例：劈天狂击 17278 [743]|[1,1]、咆哮熔岩 18029 [743]|[1,1]。
// 1491（args_num=0）"遇到天敌时先制+1"：
//   - 选择期路由：内核 loadSkills 的 **selection_effects_ 硬编码表**（2000/610/954/
//     1848/881/1532/2298/779/1860/1491——J11/J29/J53 "路由表登记一行"的正位）登记
//     1491 → on_selected 注册到 MOVE_RIGHT 桶，先制判定前执行一次；
//     ⚠️ 勘误（J66 实测）：effect_register_state case 路由（2020 形）注册进的是用技期
//     分支，赶不上首回合先制判定——条件先制族**必须**走本表，勿再加 case；
//   - 天敌判据 = is_predator（common.h：对手**固有属性**克制自身，1530/464 同款；
//     双属性组合相乘、不看技能系别）→ ws.preemptive_level[owner] += 1；
//   - 零参切片 → build_effect_args_for_skill 垫 [owner,target]（964 零参同款）。
//   - 实例：临风利爪 37332 [1491,956]|[250]（956 已注册 J12；R1 用技后附加、断言面外）。
//
// 动工前置三查（2026-10-03）：①内核 grep 743/1491 无行为硬编码（battle_primitives.h
//   743 注释仅文档引用；1491 无 case）②effect_meta 无分类牵连 ③moves_lib 全 worktree
//   并集无注册——真缺口。切片：side_effect 表 arg_count 743=2/1491=0 与
//   effect_info.args_num 一致；33 载体链切片自洽（[回合,先制] / 零参链首）。
// 内核改动：skills.cpp loadSkills 的 selection_effects_ 路由表加 1491 一项
//   （2298/779/1860 同款；与并行任务错峰动 skills.cpp——J53 先例）。
// 先例：1110（stat_boost_reversal 调用形）、J61（NextRounds 先制窗）、2020（MOVE_RIGHT
//   路由 + preemptive_level 直写）、common.h is_predator。
// 场景回归：test/scenario/scenario_1530_reversal_prio.cpp（1530 号段）。

#include "common.h"

namespace {

// ── 743 —— 反转对手能力提升，反转成功下{0}回合先制+{1} ──
// int_args = [owner, target, rounds, prio]（args_num=2）。
EffectResult effect_743_reversal_prio_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 743, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 743, args,
                     "743 布局=[owner,target,rounds,prio]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int prio = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     743, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && prio > 0, 743, args, "需要 rounds>0 且 prio>0");
    if (!g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, target) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // NOTHING（无提升）/ BLOCKED（免弱）都不算成功
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 743, rounds,
                                {owner, 1 - owner, prio}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 1491 —— 遇到天敌时先制+1（选择期，内核 case 1491 路由到 MOVE_RIGHT）──
// int_args = [owner, target]（args_num=0，bind_participants 补位）。
EffectResult effect_1491_predator_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1491, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1491, args,
                     "1491 布局=[owner,target]（零参补位）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1491, args, "owner 越界");
    if (!is_predator(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(743, effect_743_reversal_prio_window)
SKILL_EFFECT(1491, effect_1491_predator_priority)
