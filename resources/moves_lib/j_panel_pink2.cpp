// J 组 J122 · 面板粉伤族二 —— 534×13 / 670×13（2026-10-03，worktree ../agents/j122）
//
// 家族语义（effect_info 模板即权威）：
//   534（0 参）"附加防御、特防总和10%的百分比伤害"
//   670（4 参）"{0}回合，每回合附加{1}的{2}值的{3}%的百分比伤害"
//
// 实现口径：
//   - 534 = SKILL_EFFECT 即时：双防面板和（ws.getTempAbilityValue DEFENSE+SPECIAL_DEFENSE
//     ——944 同款在战面板读法）×10% → deal_pink_damage(PERCENT_VALUE)（944/676 直系，
//     J30 灿烂极光零参先例）。面板取**施放方**（944 口径）。
//   - 670 = InRounds({0}) 每回合窗（含当回合，944 同款自卡）ROUND_END tick（1655/861
//     注册形）→ 施放方面板 ×{3}% 粉打对手。模板无"攻击技能"限定词 → **非攻击门**
//     （回合末 tick，不要求当回合用过攻击技）。{1}=0 恒（自身）；{2} 面板码：0=物理攻击
//     （神威破八荒 [3,0,0,15]，4399"附加自身物理攻击15%的固定伤害"）、2=特攻（闪芒之浴
//     [3,0,2,50]，4399"附加自己特攻值的50%的固定伤害"）——其余 {2} 取值记档跳过
//     （effect_arg 现场报告，EFFECT_ARG_GUARD）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-03）：
//   534  0 参：15026 神力碎土拳（纯链）、15519 盾牌狂乱击、16108 地龙粉碎击…
//        ⚠️ 4 载体切片有残参（16597/17625 等链尾参无主，R1 台账口径），不进场景。
//   670  [回合,p1,p2,pct]：16579 神威破八荒 [3,0,0,15]（纯链）、26915 闪芒之浴
//        [3,0,2,50]、24247 幻影魔能 [2,0,0,25]、24876 凌厉若风 [3,0,0,50]…
//        （12/13 自洽，1 残参不进场景）
//
// 动工前置三查（2026-10-03）：①内核 grep 534/670 无硬编码 ②effect_meta 无分类牵连
// ③moves_lib 无既有注册——真缺口（deal_pink_damage/getTempAbilityValue 均既有设施）。
// 场景回归：test/scenario/scenario_2160_panel_pink.cpp（号段 2160-2169）。

#include "common.h"

namespace {

// ── 670 的 ROUND_END tick —— payload={owner, opp, p1, p2, pct}。
EffectResult j122_panel_pink_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int p1 = args.int_args[2];
    const int p2 = args.int_args[3];
    const int pct = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp || pct <= 0) {
        return EffectResult::kOk;
    }
    EFFECT_ARG_GUARD(p1 == 0, 670, args, "670 {1} 仅支持 0=自身（其余记档跳过）");
    NumericalPropertyIndex panel = NumericalPropertyIndex::PHYSICAL_ATTACK;
    if (p2 == 2) {
        panel = NumericalPropertyIndex::SPECIAL_ATTACK;   // 闪芒之浴 口径
    } else {
        EFFECT_ARG_GUARD(p2 == 0, 670, args, "670 {2} 仅支持 0=物攻/2=特攻（其余记档跳过）");
    }
    const int value = ctx->ws.getTempAbilityValue(owner, panel);
    const int pink = value * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 534 —— 附加防御、特防总和10%的百分比伤害 ──
// int_args = [owner, target]（0 参效果，装载器注入）。
EffectResult effect_534_panel_def_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 534, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 534, args,
                     "534 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     534, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int def_sum =
        ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE) +
        ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE);
    const int pink = def_sum * 10 / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 670 —— {0}回合，每回合附加{1}的{2}值的{3}%的百分比伤害 ──
// int_args = [owner, target, 回合, p1, p2, pct]（args_num=4）。
EffectResult effect_670_panel_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 670, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 670, args,
                     "670 布局=[owner,target,回合,p1,p2,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     670, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 670, args, "需要 回合/pct > 0");
    (void)register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 670, rounds,
                                std::vector<int>{owner, opp, args.int_args[3],
                                                 args.int_args[4], pct},
                                &j122_panel_pink_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(534, effect_534_panel_def_pink)
SKILL_EFFECT(670, effect_670_panel_pink_window)
