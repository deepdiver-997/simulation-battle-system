// J 组 J258 · 对手攻击类窗口回复族 —— 659×7 载体（2026-10-24，worktree feat/j258，
// a24-kprod）
//
// 家族语义（effect_info 模板 + 4399 七文本双证）：
//   659 [3] {0}回合内若对手使用{1}攻击，则恢复{2}点体力
//   （{1}=moves.category 码：1 物理 / 2 特殊 / 4 属性——4399 定标：幻境灭杀"若对手
//   使用物理攻击"、月读之光"若对手使用属性攻击"、鱿鱿屏障"若对手使用特殊攻击"）
//
// 结算口径（板行二选一：对手该类行动后恢复 vs 回合末统一结算）→ **回合末统一结算**
//   （ROUND_END 桶判据，§四点五 "对手行动后+未触发兜底：锚点一律 ROUND_END 桶"——
//   对手 NONE/被控回合判据天然不误触发）。对手**使用**即算（miss/被盔算使用、被控/
//   NONE 不算——J 系判据统一 2026-10-03），窗内每回合至多恢复一次。
//
// 实现口径（直系先例：h_miss_window 78/432 窗式注册 + 740 六槽/判据 + J125 平复）：
//   - register_round_effect(ROUND_END, InRounds)——起点经 round_effect_start_round
//     折算（含当回合，先/后出手对称，§五.0）；tick 读 executed（skill_use_seq 增量）
//     + **ws.skill_type_view**（实际物化类型，变型技正确——判据统一 §四点五，
//     优于静态 roundChoice 读法）映射回 category 码 {0=Physical→1, 1=Special→2,
//     2=Attribute→4}，匹配则 heal_amount(owner, {2})。
//   - heal 平回复原语（J125 回复族）；自身 hp<=0 不恢复。
//
// 载体切片（side_effect.arg_count 优先，2026-10-24 核算 7/7 自洽）：
//   纯链：17113 幻境灭杀 [659]|[2,1,80]（物150，2回合/物理/80）
//   混链：23515 据水断桥 [48,659]|[4,3,4,200] → 659 切 [3,4,200]（48=免异常 4 回合）
//     23821 女神降临 [43,659,4,4]|[2,3,1,200,1,100,2,3,100,2] → 659 切 [3,1,200]
//     23953 岩化形态 [659,5,5]|[3,1,150,…] → 659 切 [3,1,150]（5=对手双攻-1×2）
//     23996 月读之光 [659,5,5]|[3,4,150,…] → 659 切 [3,4,150]（5=速度-1/命中-1）
//     24041 细胞活化 [4,4,4,659]|[1,100,2,3,100,2,5,100,1,3,4,150] → 659 切 [3,4,150]
//     28143 鱿鱿屏障 [659,599,106]|[3,2,300,3,1,100,3] → 659 切 [3,2,300]
//     （599=受特殊伤-100% 窗、106=特殊必miss 窗；43/4/5/599/106 registry 均已实现）
//
// 动工前置三查（2026-10-24）：①内核 grep 659 无硬编码 ②effect_info 659 模板即本族
//   ③registry moves 无 659、moves_lib 全库无 SKILL_EFFECT(659)——真缺口。
// 场景回归：test/scenario/scenario_7380_opponent_category_heal_window.cpp（7380-7389）。

#include "common.h"

namespace {

// ── 659 tick —— ROUND_END：对手本回合用过 {cat} 类技能 → 恢复 {heal} 点 ──
// args = [owner, target, cat, heal]。
EffectResult j258_659_tick(BattleContext* ctx, const EffectArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = a.int_args[0];
    const int opp = a.int_args[1];
    const int cat = a.int_args[2];
    const int heal = a.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    if (heal <= 0 || ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    // "使用"判据（§四点五）：skill_use_seq 较回合初基线有增量；被控/NONE 不算。
    if (ctx->skill_use_seq[opp] <= ctx->ws.skill_use_seq_round_start[opp]) {
        return EffectResult::kOk;
    }
    // 实际物化类型 → moves.category 码（变型技正确；未知类型不触发）。
    int used_cat = -1;
    switch (ctx->ws.skill_type_view[opp]) {
        case static_cast<int>(SkillType::Physical):
            used_cat = 1;
            break;
        case static_cast<int>(SkillType::Special):
            used_cat = 2;
            break;
        case static_cast<int>(SkillType::Attribute):
            used_cat = 4;
            break;
        default:
            return EffectResult::kOk;
    }
    if (used_cat != cat) {
        return EffectResult::kOk;   // 类别不合（{1} 门）
    }
    (void)g_core->heal_amount(ctx, owner, heal);
    return EffectResult::kOk;
}

// ── 659 —— {0}回合内若对手使用{1}攻击，则恢复{2}点体力 ──
// int_args = [owner, target, {0}=回合, {1}=category 码, {2}=恢复点数]。
EffectResult effect_659_opponent_category_heal_window(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 659, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 659, args,
                     "659 布局=[owner,target,回合,cat,点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int cat = args.int_args[3];
    const int heal = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     659, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 659, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(cat == 1 || cat == 2 || cat == 4, 659, args,
                     "cat 须为 moves.category 码 1/2/4");
    EFFECT_ARG_GUARD(heal > 0, 659, args, "需要 恢复点数 > 0");
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 659, rounds,
                          {owner, opp, cat, heal}, &j258_659_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(659, effect_659_opponent_category_heal_window)
