// J69 · 后手六槽族 —— 483×18（2026-10-03，worktree ../agents/j69）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 483（args_num=6）"对手{0},后出手时弱化效果翻倍"：
//   - {0} = 语义序六槽变化向量（攻,防,特攻,特防,速,命中 → kStatMap {0,2,1,3,4,5}，
//     J48 732 同表）；载体实载全为负值（迷惑之光族统一 [-1,0,-1,0,-1,-1]）= 对手弱化；
//   - 后手判定 = SKILL_EFFECT 时点（先手权已定）：owner != 先手方 → 幅度 ×2；
//   - 负值全量 → 逐槽 stat_drop(target, slot, |delta|×倍率, owner)（**查免弱**，
//     J48 732 负值分支同款；正值容错走 stat_change——切片无正值，保守同表处理）。
//   - 实例：迷惑之光 22238 [483]|[-1,0,-1,0,-1,-1]（属性技零伤害载体，场景最干净）。
//
// 动工前置三查（2026-10-03）：①内核 grep 483 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count 483=6
//   与 effect_info.args_num 一致；18 载体全纯链、向量统一，零剩余参数。
// 先例：J48（j_crit_gate_sixslot.cpp kStatMap + stat_drop/stat_change 分支）、
//   J49 476（acting 先后手判定）、J12（门框架小工具形）。
// 场景回归：test/scenario/scenario_1560_second_sixslot.cpp（1560 号段）。

#include "common.h"

namespace {

// 语义序（攻,防,特攻,特防,速,命中）→ 引擎能力槽（J3 1823/J48 732 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

}  // namespace

// ── 483 —— 对手{0}（语义序六槽向量），后出手时弱化效果翻倍 ──
// int_args = [owner, target, 六槽×6]（args_num=6）。
EffectResult effect_483_second_sixslot_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 483, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 483, args,
                     "483 布局=[owner,target,六槽×6]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     483, args, "需要 owner/target ∈ {0,1}");
    if (!g_core) {
        return EffectResult::kOk;
    }
    // 后手判定：SKILL_EFFECT 时点先手权已定（preemptive_right 推先手侧，J49 476 同源）。
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    const int mult = (owner != first) ? 2 : 1;
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        if (delta == 0) {
            continue;
        }
        const int slot = kStatMap[i];
        if (delta < 0) {
            if (g_core->stat_drop) {
                (void)g_core->stat_drop(ctx, target, slot, -delta * mult, owner);
            }
        } else if (g_core->stat_change) {
            (void)g_core->stat_change(ctx, target, slot, delta * mult);
        }
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(483, effect_483_second_sixslot_drop)
