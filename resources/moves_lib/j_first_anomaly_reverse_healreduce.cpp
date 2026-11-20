// J 组 J194 · 先手异反/回血压制族 —— 2 宏/18 载体（2026-10-22，worktree ../agents/j194）
//
// 家族语义（effect_info 模板即权威）：
//   1295 [1] 先出手时{0}回合内免疫并反弹异常状态
//   2116 [2] 先出手时{0}回合内对手体力恢复效果减少{1}%
//
// 实现口径：
//   - 共同门 = acting_side_is_first(ctx)（common.h 先手判据；SKILL_EFFECT 时点
//     currentState 即出手侧，J61/J78 同款）。非先手 → 整体不触发。
//   - 1295 = 191 现代弹控直系：grant_immunity(owner, ANOMALY, Modern, rounds={0},
//     source_id/source_effect_id=1295) + rule_center_.grant_reflect(owner, -1, 1295,
//     target=owner, rounds={0})。免疫命中时 apply_anomaly 按 REFLECT 条目弹回施放方。
//   - 2116 = 761/2241 恢复减少窗直系：授予即生效 heal_mod_pct[opp] -= {1}，
//     并注册 ROUND_START 捕获-还原 tick（窗 [R, R+{0}-1]，过期回写 orig；ws 每回合
//     reset 会清 heal_mod_pct，故必须每回合重写）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1295 [回合]：35704 圣子之威 [3]（纯链，特 120）、36638 远迎 [3]、
//        36824 猫双斩 [2]、35194 环·神庭轰坠 [2]…（9/9 自洽）
//   2116 [回合,减疗%]：37671 沉眠 [1,50]（纯链，物 40）、37263 死寒之霜伐 [2,50]、
//        37141 荒合岳溟歌 [1,70]…（9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1295/2116 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5160_first_anomaly_reverse_healreduce.cpp（5160-5169）。

#include "common.h"

namespace {

// ── 2116 的 ROUND_START 捕获-还原 tick（761/2241 同形）──
// payload = {owner, opp, rounds, pct, orig, arm_round}
EffectResult j194_heal_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    const int orig = args.int_args[4];
    const int arm = args.int_args[5];
    if (opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ctx->heal_mod_pct[opp] =
        (ctx->roundCount <= arm + rounds - 1) ? (orig - pct) : orig;
    return EffectResult::kOk;
}

// 授予恢复减少窗（授予即生效 + ROUND_START 捕获-还原；effect_id 只作桶键）。
void j194_grant_heal_reduce_window(BattleContext* ctx, int owner, int opp, int effect_id,
                                   int rounds, int pct) {
    const int orig = ctx->heal_mod_pct[opp];
    ctx->heal_mod_pct[opp] = orig - pct;
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, effect_id, rounds + 1,
                          {owner, opp, rounds, pct, orig, ctx->roundCount},
                          &j194_heal_reduce_tick);
}

// ── 1295 —— 先出手时{0}回合内免疫并反弹异常状态 ──
// int_args = [owner, target, 回合]（args_num=1）。
EffectResult effect_1295_first_anomaly_reflect(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1295, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1295, args,
                     "1295 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1295, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1295, args, "需要 回合 > 0");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 门：先出手
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                        /*source_id=*/1295, /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1295,
                        /*tier=*/ImmunityTier::Modern);
    ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1, /*source_effect_id=*/1295,
                                    /*target=*/owner, /*rounds=*/rounds);
    return EffectResult::kOk;
}

// ── 2116 —— 先出手时{0}回合内对手体力恢复效果减少{1}% ──
// int_args = [owner, target, 回合, 减疗%]（args_num=2）。
EffectResult effect_2116_first_heal_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2116, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 2116, args,
                     "2116 布局=[owner,target,回合,减疗%]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     2116, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct >= 0 && pct <= 100, 2116, args,
                     "需要 回合>0 且 减疗%∈[0,100]");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 门：先出手
    }
    if (pct > 0) {
        j194_grant_heal_reduce_window(ctx, owner, opp, 2116, rounds, pct);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1295, effect_1295_first_anomaly_reflect)
SKILL_EFFECT(2116, effect_2116_first_heal_reduce)
