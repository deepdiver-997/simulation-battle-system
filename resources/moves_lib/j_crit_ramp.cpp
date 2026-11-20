// J146 · 暴击递增族 —— 441×12（2026-10-04，worktree ../agents/j146）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 441（args_num=2）"每次攻击提升{0}%的致命概率，最高提升{1}%"：
//   - 持久递增计数挂 plugin_storage（键 990441000+owner；**审计槽** 990441100+owner
//     同步落"本次申报概率×100"——F9 782 概率递增族口径，场景无随机精确断言）；
//   - 写入时点 = **MOVE_RIGHT tick**（NextRounds(1) 每次用技重挂）：写
//     ws.crit_rate_add[自身] += min(N×{0},{1})（加法百分点通道，内核 query_usage 掷骰
//     `rate>=100 必暴短路`；⚠️ 不能在 SKILL_EFFECT 写——暴击掷在 execute 早期
//     query_usage 内，SKILL_EFFECT 晚于它，本回合攻击吃不到）；
//   - 计数递增在 SKILL_EFFECT 本体（本技能每打一次 +1）→ 首次攻击 0 增益、次回合起
//     {0}/2{0}/… 封顶 {1}（跨回合递增语义，记档——同回合二动吃不到当次递增）。
//   -载体 crit_rate=0 也生效（内核注释：add 是引爆效果，base=0 照样能暴）。
//
// 动工前置三查（2026-10-04，立单时已核）：①内核 grep 441 无行为硬编码（crit_rate_add
//   即既有加算钩子，effect 32 蓄气族同通道）②battle_effects/custom_effect_overrides
//   无行 ③moves_lib 全 worktree 并集无注册。切片 [步长,上限] 12/12 自洽，{0}∈{10,15,20}，
//   {1}∈{30,40,50,60}；纯链×7（13655 天翔龙炎击 [10,30]/14863 [10,40]/33961 爆裂打击
//   [10,30] 等）。
// 先例：F9 782（审计槽）、843/499（MOVE_RIGHT 桶效果）、J146 前夜 J129（探针骨架）。
// 场景回归：test/scenario/scenario_2410_crit_ramp.cpp（2410 号段，审计槽梯度断言）。

#include "common.h"

namespace {

constexpr int kJ146CounterBase = 990441000;   // 递增计数（持久）
constexpr int kJ146AuditBase   = 990441100;   // 审计槽：本次申报概率×100

// MOVE_RIGHT tick：payload = [self, step, cap] → 写 crit_rate_add + 审计槽。
EffectResult j146_441_move_right_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int step = args.int_args[1];
    const int cap = args.int_args[2];
    if (self < 0 || self > 1 || step <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    const long long n = any_int_or(ctx->plugin_storage[kJ146CounterBase + self]);
    const int declared = static_cast<int>(std::min<long long>(n * step, cap));
    ctx->ws.crit_rate_add[self] += static_cast<float>(declared);
    ctx->plugin_storage[kJ146AuditBase + self] = declared * 100;   // 审计槽（×100 整化）
    return EffectResult::kOk;
}

}  // namespace

// ── 441 —— 每次攻击提升{0}%的致命概率，最高提升{1}% ──
// int_args = [owner, target, step, cap]（args_num=2）。
EffectResult effect_441_crit_ramp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 441, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 441, args,
                     "441 布局=[owner,target,步长,上限]");
    const int self = args.int_args[0];
    const int step = args.int_args[2];
    const int cap = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     441, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(step > 0 && cap > 0, 441, args, "需要 步长/上限 > 0");
    // 计数递增：本技能每执行一次 +1（持久）。
    const int key = kJ146CounterBase + self;
    ctx->plugin_storage[key] = any_int_or(ctx->plugin_storage[key]) + 1;
    // 重挂下一回合的 MOVE_RIGHT tick（NextRounds(1)：下一回合 写入、下下次再重挂）。
    // ⚠️ MOVE_RIGHT 只有单桶（BATTLE_FIRST_MOVE_RIGHT=双方先手权结算，owner 作用域分发）。
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 441,
                                /*rounds=*/1, {self, step, cap}, &j146_441_move_right_tick);
    return EffectResult::kOk;
}

SKILL_EFFECT(441, effect_441_crit_ramp)
