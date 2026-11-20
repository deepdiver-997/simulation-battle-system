// J 组 J292 · 双防回合弱化/先手自增族 —— 2 宏/14 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   150 [2] "{0}回合内对手每回合防御和特防等级{1}"
//   588 [3] "先出手时，{0}%概率使自己{1}+{2}"
//
// 实现口径：
//   - 150 = 双防逐回合结算。当回合**立即**结算一次（SKILL_EFFECT 时点；首/后手都含本回合），
//     后续 {0}-1 次由 ROUND_END tick 补（409/j_randdrain_slowhit 同款：ROUND_END 桶
//     每回合只跑一次；ROUND_START 桶实测每回合两遍、每遍扣窗时长，R2 后即过期，不可用）。
//     负数档走 `stat_drop(target, 槽, -delta, owner)` 真原语（查附加禁令/免弱/钳 -6）；
//     正数档走 `stat_change`。槽 = 防御 2 / 特防 3（能力等级槽序）。
//   - 588 = SKILL_EFFECT 时点判"先出手"（`acting_side_is_first`，currentState 在
//     FIRST_*/SECOND_* 区段；burn_trio/batch_a 同判据）→ 掷 {0}% → `stat_change(自身, {1}, {2})`
//     （本体/视图同步；自身增益不查免弱）。{0}=0 显式关闸（roll_percent(0)=false）。
//
// plugin_storage 审计槽（精确断言用；grep 全库无撞）：
//   991500000 + owner = 150 已结算次数（当回合立即 + 每次 ROUND_END tick）
//   991500100 + owner = 150 本次档位（delta，便于断言方向）
//   995880000 + owner = 588 先出手判定（1=先手）
//   995880100 + owner = 588 概率掷骰结果（1=命中）
//
// 载体切片（side_effect.arg_count 顺序切分，脚本核算 2026-10-22，7/7 + 7/7 自洽）：
//   150 [回合,幅度]：15854 穿铠巨角钻 [3,-1] / 21404 魔界之种 [3,-1]（属性纯链，场景载体）/
//     26173 寂灭之焰 [3,-1] / 26477 异界庇护 [3,-1] / 27113 暗影之法 [3,-1] /
//     27472 魔能枯萎 [3,-1] / 37902 硬化冲烈破 [3,-1]。
//   588 [概率,能力槽,幅度]：16114 战天灭魂击 [100,0,1]（纯链，场景载体）/
//     16260 天威爆裂冲 [100,0,1] / 16395 狂龙破 [100,4,1] / 16441 魔龙箴言咒 [100,2,1] /
//     17069 元素击杀 [100,4,1] / 18005 寂·万煞魂吟 [100,2,1] / 19013 剑破苍穹 [50,0,2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 150/588 无行为硬编码（150 命中项均为特性
//   Eid/注释数字，非同空间）②effect_info 模板唯一、custom_effect_overrides/programs
//   零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与各 agents worktree）无
//   SKILL_EFFECT 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6250_stat_window_priority.cpp（号段 6250-6259）。

#include "common.h"

namespace {

constexpr int k150ApplyAuditBase = 991500000;  // +owner：已结算次数
constexpr int k150DeltaAuditBase = 991500100;  // +owner：本次档位
constexpr int k588FirstAuditBase = 995880000;  // +owner：先手判定
constexpr int k588RollAuditBase = 995880100;   // +owner：掷骰结果

// ── 150 结算体：防御/特防各一次 ──
inline void apply_def_spdef(BattleContext* ctx, int owner, int target, int delta) {
    if (!ctx || owner < 0 || owner > 1 || target < 0 || target > 1 || delta == 0 || !g_core) {
        return;
    }
    for (const int stat : {/*防御=*/2, /*特防=*/3}) {
        if (delta < 0) {
            if (g_core->stat_drop) {
                (void)g_core->stat_drop(ctx, target, stat, -delta, owner);
            }
        } else if (g_core->stat_change) {
            (void)g_core->stat_change(ctx, target, stat, delta);
        }
    }
}

// ── 150 的 ROUND_END tick —— payload={owner,target,delta,r0,rounds} ──
EffectResult j292_150_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int delta = args.int_args[2];
    const int r0 = args.int_args[3];
    const int rounds = args.int_args[4];
    if (ctx->roundCount <= r0 || ctx->roundCount > r0 + rounds - 1) {
        return EffectResult::kOk;   // 回合界 [r0+1, r0+{0}-1]（tick 内自判）
    }
    apply_def_spdef(ctx, owner, target, delta);
    const auto it = ctx->plugin_storage.find(k150ApplyAuditBase + owner);
    ctx->plugin_storage[k150ApplyAuditBase + owner] =
        (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0)) + 1;
    return EffectResult::kOk;
}

// ── 150 —— {0}回合内对手每回合防御和特防等级{1} ──
// int_args = [owner, target, 回合, 幅度]（args_num=2）。
EffectResult effect_150_def_spdef_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 150, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 150, args,
                     "150 布局=[owner,target,回合,幅度]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     150, args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：0 回合/0 幅度 = 显式关闸档（不回退默认值）。
    ctx->plugin_storage[k150ApplyAuditBase + owner] = 0;
    ctx->plugin_storage[k150DeltaAuditBase + owner] = delta;
    if (rounds <= 0 || delta == 0) {
        return EffectResult::kOk;
    }
    // 当回合立即结算（ROUND_END 桶本回合在技能之后才跑，但 NextRounds 窗从下回合起算
    // → 含当回合必须在技能时点内联，409 同款）。
    apply_def_spdef(ctx, owner, target, delta);
    ctx->plugin_storage[k150ApplyAuditBase + owner] = 1;
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_END, owner, 150, rounds - 1,
                                    {owner, target, delta, ctx->roundCount, rounds},
                                    &j292_150_tick);
    }
    return EffectResult::kOk;
}

// ── 588 —— 先出手时，{0}%概率使自己{1}+{2} ──
// int_args = [owner, target, 概率, 能力槽, 幅度]（args_num=3）。
EffectResult effect_588_first_move_self_stat(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 588, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 588, args,
                     "588 布局=[owner,target,概率,能力槽,幅度]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int stat = args.int_args[3];
    const int delta = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     588, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(stat >= 0 && stat < BattleContext::kAbilityLevelSlotCount, 588, args,
                     "能力槽越界（0..5）");
    const bool first = acting_side_is_first(ctx);
    ctx->plugin_storage[k588FirstAuditBase + owner] = first ? 1 : 0;
    ctx->plugin_storage[k588RollAuditBase + owner] = 0;
    // 先出手门 + {0}% 概率门；pct<=0 显式关闸（roll_percent 内判）。
    if (!first || delta == 0 || !roll_percent(pct)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k588RollAuditBase + owner] = 1;
    if (g_core && g_core->stat_change) {
        (void)g_core->stat_change(ctx, owner, stat, delta);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(150, effect_150_def_spdef_window)
SKILL_EFFECT(588, effect_588_first_move_self_stat)
