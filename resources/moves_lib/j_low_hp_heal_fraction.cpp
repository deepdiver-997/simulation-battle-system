// J 组 J245 · 低体回复族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1093（args_num=1）"出手时若自身体力低于对手则恢复自身最大体力的1/{0}"
//
// 实现口径：
//   - 门 = 出手时**当前体力严格低于**对手当前体力（`hp < hp`；相等不触发）。
//   - 落点 = `heal_max_hp_fraction(自身, {0})` —— 恢复量 = 最大体力 / {0}，走
//     heal_amount 原语（吃封回血/恢复修正；满血自然钳 0）。与 704/541 同族。
//   - 时点 = SKILL_EFFECT（"出手时"；当次技能链先结算完再判定，比分按判定时现场值）。
//   - `{0}` 在位即消费：<=0 = 不回复（不回退默认值）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   28329 [1211,48,1093]|[300,3,3] / 30372 [889,1093]|[0,3] /
//   31369 [1114,1093]|[1,3] / 35280 [1114,1360,1093]|[1,100,3] /
//   **35382 异兽电流 [1093]|[3]（纯链，场景载体）** /
//   35586 [1070,1360,1093]|[100,3] / 37399 [1037,1093]|[1,50,3] /
//   37892 [1201,909,1093]|[100,8,1,2,100,0,300,3]。
//   全库实载 {0} ∈ {3,8}。
//
// 动工前置三查（2026-10-22）：①内核 grep 1093 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5760_low_hp_heal_fraction.cpp（号段 5760-5769）。

#include "common.h"

namespace {

// ── 1093 —— 出手时若自身体力低于对手则恢复自身最大体力的1/{0} ──
EffectResult effect_1093_low_hp_heal_fraction(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1093, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1093, args,
                     "1093 布局=[owner,target,denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1093,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：显式 0/负档 = 不回血（不回退默认值）。
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    // "体力低于对手" = 当前体力**严格小于**（相等不触发）。
    if (ctx->getPet(owner).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1093, effect_1093_low_hp_heal_fraction)
