// J 组 J155 · 窗降/窗吸族 —— 725×11 / 13×10（2026-10-04，worktree ../agents/j155）
//
// 家族语义（effect_info 模板即权威）：
//   725（args_num=3）"{0}回合内每回合有{1}%的概率降低对手最大体力的1/{2}"
//     —— 万世荒芜决 17072 [3,30,3]（纯链特）。
//   13 （args_num=1）"{0}回合吸取对方最大体力的1/8(对草系无效)"
//     —— 寄生种子 20007 [5]（纯链属）、毒蔓抽离 35835 [5]（纯链特2）。
//
// 实现口径：
//   - 725 = NextRounds({0}) ROUND_START tick（J119/J154 同族；⚠️ 中途注册用 NextRounds
//     而非 InRounds——本回合 ROUND_START 桶已过，InRounds 会少一跳，NextRounds 恰
//     {0} 跳，记档）→ roll_percent({1}) → reduce_max_hp_pct(对手 on_stage, 100/{2},
//     floor=1)（1028/1102 直系）。
//   - 13 = NextRounds({0}) ROUND_START tick → drain_hp(自身, 对手, 8)（CoreApi 163
//     槽，1/8 对手最大体力和吸血一体）；草系门 = 对手系别视图含官方码 1（草，
//     trait_state.h 官方属性表）→ 跳过（每跳现查，系别可被改系类效果改写）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   725 [回合,概率,分母]：17072 万世荒芜决 [3,30,3]（纯链）
//   13  [回合]：20007 寄生种子 [5]（纯链属）、35835 毒蔓抽离 [5]（纯链）
//
// 动工前置三查（2026-10-04）：①内核 grep 725/13 无硬编码 ②effect_meta 无分类牵连
// ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2570_window_drain.cpp（号段 2570-2579）。

#include "common.h"

namespace {

constexpr int kGrassElement = 1;   // 官方属性 id：1 = 草（trait_state.h 官方属性表）

bool j155_is_grass(const BattleContext* ctx, int side) {
    const int* elem = ctx->ws.view_elementalAttributes[side];
    return elem[0] == kGrassElement || elem[1] == kGrassElement;
}

// ── 725 的 ROUND_START tick —— payload={owner, target, chance, denom}。
EffectResult j155_max_hp_down_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->reduce_max_hp_pct) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target || chance <= 0 ||
        denom <= 0) {
        return EffectResult::kOk;
    }
    if (!roll_skill_percent(*ctx, 725, 1, owner, target, chance)) {
        return EffectResult::kOk;
    }
    (void)g_core->reduce_max_hp_pct(ctx, target, ctx->on_stage[target], 100 / denom,
                                    /*floor=*/1);
    return EffectResult::kOk;
}

// ── 725 —— {0}回合内每回合有{1}%的概率降低对手最大体力的1/{2} ──
// int_args = [owner, target, rounds, chance, denom]（args_num=3）。
EffectResult effect_725_max_hp_down_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 725, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 725, args,
                     "725 布局=[owner,target,回合,概率,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     725, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && denom > 0, 725, args,
                     "需要 回合/概率/分母 > 0");
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 725, rounds,
                                      std::vector<int>{owner, target, chance, denom},
                                      &j155_max_hp_down_tick);
    return EffectResult::kOk;
}

// ── 13 的 ROUND_START tick —— payload={owner, target}。
EffectResult j155_parasite_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target) {
        return EffectResult::kOk;
    }
    if (j155_is_grass(ctx, target)) {
        return EffectResult::kOk;   // 对草系无效（每跳现查系别视图）
    }
    (void)g_core->drain_hp(ctx, owner, target, /*fraction_denom=*/8);
    return EffectResult::kOk;
}

// ── 13 —— {0}回合吸取对方最大体力的1/8(对草系无效) ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_13_parasite_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 13, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 13, args,
                     "13 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     13, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 13, args, "需要 回合 > 0");
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 13, rounds,
                                      std::vector<int>{owner, target},
                                      &j155_parasite_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(725, effect_725_max_hp_down_window)
SKILL_EFFECT(13, effect_13_parasite_window)
