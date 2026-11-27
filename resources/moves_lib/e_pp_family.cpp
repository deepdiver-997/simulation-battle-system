// E6 · PP 族批量效果 598（引用 49 次，2026-09-29，分支 feat/e-E6）
//
// 598 "{0}%恢复自己所有技能PP值{1}点"（args_num=2，切片如 明语释心 28449 → [100, 5]）：
//   - SKILL_EFFECT 即时结算（链内；{0}% 概率门在**使用时掷一次**，受管 roll_skill_percent
//     ——E4 的 127 同口径，场景 103 概率闸门管辖）；
//   - 恢复走 pp_restore 原语：clamp ≤ maxPP、无限 PP 槽跳过、只补不削
//     （极渊 2414"20%自身全技能 PP+1"/D22 1821"未击败全技能+5PP"同款）；
//   - only_empty=false：598 是"所有技能恢复{1}点"，不是亮节"只补 pp==0 的槽"。
//
// 动工三查（2026-09-29）：内核无 598 硬编码；effect_meta 无特殊分类；
// 全库无既有实现（87 是"回满"无概率参数、39 是降 PP 族，均不同模板）。
//
// 场景回归：test/scenario/scenario_510_pp_family_598.cpp。

#include "common.h"

namespace {

// ── 598 —— {0}%恢复自己所有技能PP值{1}点 ──
EffectResult effect_598_prob_restore_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 598, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 598, args,
                     "598 布局=[owner,占位,prob,points]（int_count>=4）");
    const int owner = args.int_args[0];
    const int prob = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 598, args, "owner 越界");
    EFFECT_ARG_GUARD(points > 0, 598, args, "points 应为正");
    if (!g_core || !g_core->pp_restore) {
        return EffectResult::kOk;
    }
    if (!roll_skill_percent(*ctx, 598, 0, owner, owner, prob)) {
        return EffectResult::kOk;   // 概率未命中：不恢复
    }
    (void)g_core->pp_restore(ctx, owner, points, /*only_empty=*/false);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(598, effect_598_prob_restore_pp);
