// J 组 J256 · 下降镜像返还族 —— 706×6 载体（2026-10-24，worktree feat/j256，a24-kprod）
//
// 家族语义（effect_info 模板 + 4399 载体文本双证）：
//   706 [0] 将自身的能力下降状态{0}倍返还给对手
//
// 触发时点口径（板行二选一："生效即返还" vs "对手下次降我后返还"）：
//   4399 六载体文本均为**使用技能时结算**句式，无"之后/下回合"字样——江河倒灌
//   "必中；将自身能力下降状态双倍反馈给对手"、极限梦幻/喵喵咒语"将自身的能力下降
//   状态2倍返还给对手"、天佑明尊/灼锋破晦混链同句式 → **生效即返还**：SKILL_EFFECT
//   时点读自身当前负槽，逐槽即时返还（740"每回合反馈"是窗式持续族，与本族不同形）。
//
// 实现口径（直系先例：722 镜像赋予反向形 + 740 反馈窗的六槽扫描）：
//   - 仅**负**等级槽触发；返还幅度 = |自身负等级| × args[0]；自身下降**不清除**
//     （"返还"非"转移"，模板无清除子句——同 722"赋予不是转移"推理）。
//   - stat_drop 原语内查对手免弱（STAT_DROP，J48 负值分支口径）/附加禁令、钳 -6；
//     actor=属主如实声明。
//
// 载体切片（side_effect.arg_count 优先，2026-10-24 核算 6/6 自洽）：
//   纯链 [706]：18334 江河倒灌[2]（必中特攻150）/ 32316 极限梦幻[2]（物130）/
//     33257 烈火燃烧[2]（必中特攻130）/ 33497 喵喵咒语[2]（特攻140）
//   混链：23837 天佑明尊 [48,706,707] args[5,2,4,3,3] → 706 切 **[2]**
//     （48=5回合免异常窗、707=4回合1/3恢复窗——registry 均已实现，链全活）；
//     37793 灼锋破晦 [2220,706,2200] args[10,2,20] → 2220 arg_count=0 不吃参、
//     **706 切 [10]**（4399"10倍返还"旁证 ✓）、2200 切 [2,20]（未实现，良性缺席）。
//
// 动工前置三查（2026-10-24）：①内核 grep 706 无硬编码 ②effect_info 706 模板即本族
//   （唯一模板句）③registry moves 无 706、moves_lib 全库无 SKILL_EFFECT(706) 注册
//   ——真缺口。
// 场景回归：test/scenario/scenario_7330_drop_return_family.cpp（7330-7339）。

#include "common.h"

namespace {

// ── 706 —— 自身负等级逐槽按 {0} 倍返还为对手同槽下降 ──
// int_args = [owner, target, 倍数]（arg_count=1）。
EffectResult effect_706_return_drops(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 706, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 706, args,
                     "706 布局=[owner,target,倍数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int mult = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     706, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(mult > 0, 706, args, "需要 倍数 > 0");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        const int level = ctx->ability_levels[owner][slot];
        if (level < 0) {
            // 同槽 {0} 倍返还；免弱/附加禁令/钳 -6 全在原语内。
            (void)g_core->stat_drop(ctx, opp, slot, -level * mult, owner);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(706, effect_706_return_drops)
