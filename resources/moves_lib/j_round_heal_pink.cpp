#include <utils/sbs_rand.h>
// J 组 J30 · 回合恢复/攻击附粉族 —— 2 宏/42 载体（2026-10-02，worktree ../agents/j30）
//
// 家族语义（effect_info 模板即权威）：
//   651 [3] {0}回合内每回合随机恢复{1}-{2}点体力
//   676 [2] 下{0}回合每回合使用攻击技能附加自身防御、特防值总和{1}%的百分比伤害
//
// 实现口径：
//   - 651 = InRounds {0} 窗 + ROUND_START tick heal_amount(rand∈[{1},{2}])（597 窗族 +
//     区间掷点 roll）。**累加型非幂等**（J19 759 同坑）：立即恢复只在窗口含当回合时做
//     （round_effect_start_round 对后手顺延折算，双写会多回一次）。
//   - 676 = EVENT_SKILL_USED watcher（board 处方：E13 469 注册法；filter ev.actor==self
//     && !ev.used_attribute = "使用攻击技能"——使用即触发，miss 也算）→
//     deal_pink_damage(ev.target, (DEF+SDEF 视图值)×{1}/100, PERCENT_VALUE)。
//     双防读 **视图槽**（ws.getTempAbilityValue，含能力等级——2501 特攻值同口径，
//     "与伤害公式同源"；若官方按面板 numericalProperties 算，改这里一行即可，记档）。
//     ⚠️ J25 实测坑复用：事件中心 watcher 不防未来锚点 → 锚当前回合、时长
//     rounds+(start-current)，lambda 内自卡 [start, start+rounds)。
//
// 载体切片（unity 库 json_each 精确匹配 21+21=42；side_effect.arg_count 切分全载体
// 自洽、零剩余，脚本核算 2026-10-02）：
//   651  [rounds,lo,hi]：27602 稻香之息 [3,200,400]（测试载体）、28827 无商不尖 [4,666,888]
//   676  [rounds,pct]：23663 地裂山塌 [3,30]（测试载体）、26111 远古木灵 [3,40]
//
// 动工前置三查（2026-10-02）：①内核 grep 2 id 无硬编码 ②custom_effect_programs/overrides
// 无声明 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1150_round_heal_pink.cpp（号段 1150-1159）。

#include "common.h"

#include <cstdlib>

namespace {

// ── 651 的 ROUND_START tick —— payload={owner, lo, hi}；rand∈[lo,hi] 恢复。
EffectResult j30_651_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int lo = args.int_args[1];
    const int hi = args.int_args[2];
    if (owner < 0 || owner > 1 || lo < 0 || hi < lo) {
        return EffectResult::kOk;
    }
    const int roll = lo + (hi > lo ? ctx->rng().next() % (hi - lo + 1) : 0);
    if (roll > 0) {
        (void)g_core->heal_amount(ctx, owner, roll);
    }
    return EffectResult::kOk;
}

// ── 651 —— {0}回合内每回合随机恢复{1}-{2}点体力 ──
// int_args = [owner, target, rounds, lo, hi]（args_num=3）。
EffectResult effect_651_roundly_random_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 651, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 651, args,
                     "651 布局=[owner,target,回合,下限,上限]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int lo = args.int_args[3];
    const int hi = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     651, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && lo >= 0 && hi >= lo, 651, args, "需要 回合>0 且 0≤下限≤上限");
    // 累加型恢复：立即恢复只在窗口含当回合时做（后手顺延折算，J19 759 同防双落）。
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    if (start == ctx->roundCount) {
        j30_651_round_tick(ctx, EffectArgs(std::vector<int>{owner, lo, hi}));
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 651, rounds,
                          {owner, lo, hi}, &j30_651_round_tick);
    return EffectResult::kOk;
}

// ── 676 的 EVENT_SKILL_USED watcher 体 ──
void j30_676_on_skill_used(BattleContext* c, const BattleEvent& ev, int owner, int pct,
                           int start, int expire) {
    if (!c || c->roundCount < start || c->roundCount >= expire) {
        return;   // "下{0}回合"窗（NextRounds，起点折算后显式自卡）
    }
    if (ev.actor != owner || ev.used_attribute || ev.target < 0 || ev.target > 1) {
        return;   // 条件：自身使用攻击技能（属性技不算）
    }
    if (!g_core || !g_core->deal_pink_damage) {
        return;
    }
    // "自身防御、特防值总和"：视图槽（含能力等级，2501 同口径，见文件头记档）。
    const int def_sum = c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE)
        + c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE);
    const int pink = def_sum * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(c, ev.target, pink, DamageKind::PERCENT_VALUE, owner);
    }
}

// ── 676 —— 下{0}回合每回合使用攻击技能附加自身防御、特防值总和{1}%的百分比伤害 ──
// int_args = [owner, target, rounds, pct]（args_num=2）。
EffectResult effect_676_attack_def_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 676, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 676, args,
                     "676 布局=[owner,target,回合,pct]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     676, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 676, args, "需要 回合/pct > 0");
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    const int expire = start + rounds;
    // 事件中心不防未来锚点（J25 记档）→ 锚当前回合、时长跨到窗末，lambda 自卡窗口。
    const int duration = (start - ctx->roundCount) + rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, duration, /*once=*/false,
        [owner, pct, start, expire](BattleContext* c, const BattleEvent& ev) {
            j30_676_on_skill_used(c, ev, owner, pct, start, expire);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(651, effect_651_roundly_random_heal)
SKILL_EFFECT(676, effect_676_attack_def_pink_window)
