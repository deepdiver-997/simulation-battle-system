// J 组 J229 · 强化镜像赋予族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   722 [0] 将自身相应的能力提升赋予给对手相应的能力下降
//
// 实现口径（直系先例 j_boost_double_mirror.cpp 1177）：
//   - 逐槽遍历自身六槽（0攻/1特攻/2防/3特防/4速/5命中）；仅正等级槽触发。
//   - "相应的能力下降" = 对手**同槽**走 stat_drop(opp, slot, 自身正等级, owner)：
//     弱化原语查对手免弱（STAT_DROP）与附加禁令、钳 -6；actor=属主如实声明。
//   - 自身提升**不清除**（"赋予"不是"转移"；模板无清除子句）。
//   - 0 参效果（args_num=0 → 装载器注入 owner/target）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽，全部 0 参）：17359 大地哀嚎击（纯链物理 150，场景载体）/ 36101 食心月环 /
//   36265 迷毒蛰击 / 36501 反冲切割（三条纯链）/ 17062 无涯海神斩 / 19361 寰宇天芒斩 /
//   31902 高贵红桃 / 32216 娇蛮方块。
//
// 动工前置三查（2026-10-22）：①内核 grep 722 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5600_boost_mirror_to_foe_drop.cpp（号段 5600-5609）。

#include "common.h"

namespace {

// ── 722 —— 自身正等级逐槽镜像为对手同槽等量下降 ──
// int_args = [owner, target]（0 参效果）。
EffectResult effect_722_mirror_boosts_as_foe_drops(BattleContext* ctx,
                                                   const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 722, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 722, args,
                     "722 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 722,
                     args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        const int level = ctx->ability_levels[owner][slot];
        if (level > 0) {
            // 同槽等量弱化；免弱/附加禁令/钳 -6 全在原语内。
            (void)g_core->stat_drop(ctx, opp, slot, level, owner);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(722, effect_722_mirror_boosts_as_foe_drops)
