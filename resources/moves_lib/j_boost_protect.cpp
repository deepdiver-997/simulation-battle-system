// J29 · 强化保护/无强化先制族 —— 1683×20 / 1860×20（2026-10-02，worktree ../agents/j29）
//
// 家族语义（effect_info 官方模板），两条都围绕"自身能力提升状态"做防御/机动：
//
// 1683（arg_count=1）"{0}回合内自身的能力提升状态无法被消除或吸取"：
//   - 双票窗口：grant_immunity(STAT_CLEAR) 挡**消除**（clear_stat_boosts 入口
//     is_immune(STAT_CLEAR) 短路）+ grant_immunity(STAT_BOOST) 挡**吸取/搬走**
//     （transfer_stat_boosts 的 to 侧与 from 侧各查 STAT_BOOST——battle_primitives
//     1560/1670 段）。counts=0 窗口型（1194 STAT_DROP 票同形）、InRounds {0} 回合
//     （授予回合起算，含当回合）、ON_STAGE（换宠即失——保护随在场宠走）。
//   - 实例：固化力场 28755 [3]（全库 20 载体，属性技载体为主）。
// 1860（arg_count=0）"自身不处于能力提升状态时先制+1且必定命中"：
//   - **2298 同款选择期路由**（skills.cpp 路由表 2026-10-02 加 1860 一行——J11
//     2298/779 先例）：点击即判定，先手权比较前生效；六槽无正等级 →
//     ws.preemptive_level[自身] += 1 + ws.must_hit_grant[自身] = true（必中凭证
//     在 query_usage 物化消费——MOVE_RIGHT 是掷点前最后一个可执行时点，681 必中
//     tick 同通道）；args[0/1] 由 on_selected 绑定 owner/对手。
//   - 实例：天奔零式 36133、大雨滂沱 28090（全库 20 载体，零参）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1683 无硬编码；1860 需选择期路由表登记
//   （内核一行，2298/779 同款先例，见 skills.cpp 2026-10-02 J29 注）②effect_meta
//   无分类牵连 ③moves_lib 无既有注册——真缺口。载体数核对：20/20；全量切片核验
//   40 载体布局零异常（1683 [rounds] / 1860 空参）。
// 先例：1194/STAT_DROP（grant_immunity 票形）、2000（选择期条件先制祖模板）、
//   2298/779（选择期路由 + 无提升判据）、681（must_hit_grant 通道）、场景 001
//   （条件先制翻盘断言法）。
// 场景回归：test/scenario/scenario_1140_boost_protect.cpp（1140 号段，一文件两宏）。

#include "common.h"

namespace {

// ── 1683 —— {0}回合内自身的能力提升状态无法被消除或吸取 ──
EffectResult effect_1683_boost_protect_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1683, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1683, args,
                     "布局=[owner,target,rounds]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1683, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 1683, args, "需要 rounds>0");
    // 双票：STAT_CLEAR 挡消除、STAT_BOOST 挡吸取/搬走（窗口型 counts=0，1194 同形）。
    ctx->grant_immunity(self, ImmunityType::STAT_CLEAR, coverage_all(),
                        /*anomaly_mask=*/0, rounds, /*source_id=*/1683,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1683);
    ctx->grant_immunity(self, ImmunityType::STAT_BOOST, coverage_all(),
                        /*anomaly_mask=*/0, rounds, /*source_id=*/1683,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1683);
    return EffectResult::kOk;
}

// ── 1860 —— 自身不处于能力提升状态时先制+1且必定命中（零参，选择期路由）──
EffectResult effect_1860_no_boost_preempt_hit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1860, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1860, args,
                     "布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1860, args, "需要 owner ∈ {0,1}");
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[self][i] > 0) {
            return EffectResult::kOk;   // 处于能力提升状态 → 不触发
        }
    }
    ctx->ws.preemptive_level[self] += 1;
    ctx->ws.must_hit_grant[self] = true;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1683, effect_1683_boost_protect_window)
SKILL_EFFECT(1860, effect_1860_no_boost_preempt_hit)
