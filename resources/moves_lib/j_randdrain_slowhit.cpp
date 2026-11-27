#include <utils/sbs_rand.h>
// J 组 J73 · 随机削血/降速命中族 —— 2 宏/31 载体（2026-10-03，worktree ../agents/j73）
//
// 家族语义（effect_info 模板即权威）：
//   140 [2] 随机降低对手1/{0}至1/{1}的当前体力值
//   409 [2] {0}回合内对手每回合速度和命中等级{1}
//
// 实现口径：
//   - 140 = denom 均匀取 [{1}..{0}]（插件本地 sbs_rand 区间；切片 [8,2] → 1/8~1/2）
//     → deal_pink_damage(当前体力/denom, FIXED)（J50 644 当前形，实扣前快照）。
//   - 409 = 当回合直写（SKILL_EFFECT 内 stat_change 速槽4/命中槽5 各 {1}）+
//     ROUND_END NextRounds({0}-1) tick 后续每回合回合末重写（J67 实证 ROUND_END 桶
//     每回合只跑一次；ROUND_START 桶实测每回合两遍且每遍扣窗口时长——R2 后即过期，
//     不可用；首回合必须内联：ROUND_START 本回合已跑过，1024/850 首跳先例；
//     {0}=1 时不注册 tick）。±6 钳在 stat_change 原语内。
//
// 载体切片（unity 库 json_each 精确匹配 15+16 计账；切片全自洽）：
//   140 [上界分母,下界分母]：纯链 21342 惩戒之光 [8,2]（属性技）/22999 [5,2]/
//        23511 [3,2]/33839 [8,2]（特140）等 8 条
//   409  [回合,幅度]：纯链 17336 背影突击 [3,-1]（物85，PP20）/26141 [3,-1]/27618 [3,-1]
//
// 动工前置三查（2026-10-03）：①内核 grep 140/409 无硬编码 ②custom_effect_overrides
// 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1600_randdrain_slowhit.cpp（号段 1600-1609）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include <cstdlib>

namespace {

// ── 140 —— 随机降低对手1/{0}至1/{1}的当前体力值 ──
// int_args = [owner, target, max_denom, min_denom]（args_num=2）。
EffectResult effect_140_random_fraction_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 140, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 140, args,
                     "140 布局=[owner,target,上界分母,下界分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int max_denom = args.int_args[2];
    const int min_denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     140, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(min_denom > 0 && max_denom >= min_denom, 140, args,
                     "需要 1 <= 下界分母 <= 上界分母");
    const int span = max_denom - min_denom + 1;
    const int denom = min_denom + (span > 1 ? ctx->rng().next() % span : 0);
    const int cur = ctx->getPet(target).hp;   // 实扣前快照
    const int amount = cur / denom;
    if (amount > 0 && cur > 0 && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 409 的回合末 tick —— payload={owner, target, delta, r0, rounds} ──
EffectResult j73_409_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    const int delta = args.int_args[2];
    const int r0 = args.int_args[3];
    const int rounds = args.int_args[4];
    if (delta == 0 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount <= r0 || ctx->roundCount > r0 + rounds - 1) {
        return EffectResult::kOk;   // 回合界 [r0+1, r0+{0}-1]（tick 内自判，不依赖窗口语义）
    }
    (void)g_core->stat_change(ctx, target, /*速槽=*/4, delta);
    (void)g_core->stat_change(ctx, target, /*命中槽=*/5, delta);
    return EffectResult::kOk;
}

// ── 409 —— {0}回合内对手每回合速度和命中等级{1} ──
// int_args = [owner, target, rounds, delta]（args_num=2）。
EffectResult effect_409_speed_hit_down_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 409, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 409, args,
                     "409 布局=[owner,target,回合,幅度]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     409, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && delta != 0, 409, args, "需要 回合>0 且幅度≠0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    // 首回合内联（ROUND_START 本回合已跑过，1024/850 首跳先例）
    (void)g_core->stat_change(ctx, target, 4, delta);
    (void)g_core->stat_change(ctx, target, 5, delta);
    // 后续回合回合末 tick：ROUND_END 桶每回合只跑一次（J67 实证；ROUND_START 桶
    // 实测每回合两遍且每遍扣窗口时长——R2 后即过期，不可用）→ NextRounds({0}-1) 窗，
    // tick 落在 r0+1..r0+{0}-1 的回合末；{0}=1 时 rounds=0 → 不注册
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, owner, 409, rounds - 1,
                                    {owner, target, delta, ctx->roundCount, rounds},
                                    &j73_409_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(140, effect_140_random_fraction_drain)
SKILL_EFFECT(409, effect_409_speed_hit_down_window)
