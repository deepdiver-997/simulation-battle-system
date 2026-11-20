// J 组 J45 · 低体先手免伤族 —— 1 宏/19 载体（2026-10-02，worktree ../agents/j45）
//
// 家族语义（effect_info 模板即权威）：
//   972 [1] 出手时若自身体力低于对手，则免疫下{0}次受到的攻击伤害
//
// 实现口径：
//   - SKILL_EFFECT（施放当回合即判"出手时"）hp 对比（我 < 对手在场宠）→ 570/2096
//     DAMAGE counts 票：grant_immunity(self, DAMAGE, coverage_all(), anomaly_mask=0,
//     duration_rounds=0, counts={0}, ON_STAGE)——次数型（响应即扣，扣完自销）。
//
// 载体切片（unity 库 json_each 精确匹配 19；side_effect.arg_count 切分全载体自洽，
// 脚本核算 2026-10-02；样例 19723 [1]、28345 [1]——全族 {0}=1）：
//
// 动工前置三查（2026-10-02）：①内核 grep 972 无硬编码 ②custom_effect_overrides 无声明
// ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1300_lowhp_first_immune.cpp（号段 1300-1309）。

#include "common.h"

namespace {

// ── 972 —— 出手时若自身体力低于对手，则免疫下{0}次受到的攻击伤害 ──
// int_args = [owner, target, counts]（args_num=1）。
EffectResult effect_972_lowhp_first_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 972, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 972, args,
                     "972 布局=[owner,target,次数]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     972, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(counts > 0, 972, args, "需要 次数 > 0");
    // "出手时"：我方在场宠 hp < 对手在场宠 hp（当前值直比）。
    if (ctx->getPet(self).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/972,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/972);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(972, effect_972_lowhp_first_immune)
