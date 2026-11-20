// J106 · 属性和粉伤族 —— 1186×14 / 1163×13（2026-10-03，worktree ../agents/j106）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1186（args_num=2）"{0}回合内每回合攻击附加自身双防值总和{1}%的百分比伤害"：
//   - J30 676 直系（语义逐字同族）：EVENT_SKILL_USED watcher（NextRounds 窗自卡
//     start/expire）+ ev.actor==owner + !used_attribute（攻击技）门 →
//     deal_pink_damage(PERCENT_VALUE, 双防视图和×{1}%)；
//   - "双防值总和" = 视图槽（含能力等级，2501 同口径）DEFENSE+SPECIAL_DEFENSE。
//   - 实例：魔钢之盾 [1397,57,1186]|[1,3,4,3,3,30]（1397/57 自侧良性已注册；
//     1186=[3,30]）。
// 1163（args_num=3）"附加自身特攻值与速度值总和{0}%的百分比伤害，每次使用增加{1}%，
//   最高{2}%"：
//   - 无窗直通（SKILL_EFFECT）：面板和（SPECIAL_ATTACK+SPEED 视图值）×pct% →
//     PERCENT_VALUE 粉伤打对手；pct = {0} + 连用次数×{1}，clamp {2}
//     （plugin_storage 计数 994 键段锚 owner——每次使用 +1，无重置子句按总数记档）。
//   - 实例：灿烂极光 37306 [1163]|[20,10,40]（特130 纯链）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1186/1163 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1186=2/1163=3 与 effect_info.args_num 一致；27 载体切片自洽。
// 存储键段：1163 连用计数 990116300+owner（grep 全库无撞）。
// 先例：J30 676（j_round_heal_pink.cpp 全套：SKILL_USED watcher/NextRounds 自卡窗/
//   视图槽面板读法）、f_bonus_damage（面板数值槽口径）、J52 1328（连用计数）。
// 场景回归：test/scenario/scenario_2000_panel_pink.cpp（2000 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kCounter1163Base = 990116300;   // +owner：1163 连用计数

// 676/1186 共用的 EVENT_SKILL_USED watcher 体（J30 原形：NextRounds 自卡窗）。
void j106_on_skill_used(BattleContext* c, const BattleEvent& ev, int owner, int pct,
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
    // "双防值总和"：视图槽（含能力等级，2501 同口径）。
    const int def_sum = c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::DEFENSE)
        + c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPECIAL_DEFENSE);
    const int pink = def_sum * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(c, ev.target, pink, DamageKind::PERCENT_VALUE, owner);
    }
}

}  // namespace

// ── 1186 —— {0}回合内每回合攻击附加自身双防值总和{1}%的百分比伤害 ──
// int_args = [owner, target, rounds, pct]（args_num=2）。
EffectResult effect_1186_def_pink_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1186, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1186, args,
                     "1186 布局=[owner,target,回合,pct]");
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1186, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 1186, args, "需要 回合/pct > 0");
    const int start = ctx->round_effect_start_round(owner, rounds, EffectWindowKind::NextRounds);
    const int expire = start + rounds;
    // 事件中心不防未来锚点（J25 记档）→ 锚当前回合、时长跨到窗末，lambda 自卡窗口。
    const int duration = (start - ctx->roundCount) + rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, duration, /*once=*/false,
        [owner, pct, start, expire](BattleContext* c, const BattleEvent& ev) {
            j106_on_skill_used(c, ev, owner, pct, start, expire);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1163 —— 附加特攻+速度面板和{0}%粉伤，每次使用增加{1}%，最高{2}% ──
// int_args = [owner, target, base, inc, cap]（args_num=3）。
EffectResult effect_1163_panel_pink_growing(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1163, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1163, args,
                     "1163 布局=[owner,target,base,inc,cap]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int inc = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1 && self != target,
                     1163, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base > 0 && inc > 0 && cap > 0, 1163, args, "需要 base/inc/cap > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 连用计数（每次使用 +1；本条应用第 uses 次的 pct = base + (uses-1)×inc，clamp cap）。
    const int key = kCounter1163Base + self;
    const int uses = any_int_or(ctx->plugin_storage[key]) + 1;
    ctx->plugin_storage[key] = uses;
    int pct = base + (uses - 1) * inc;
    if (pct > cap) {
        pct = cap;
    }
    // "特攻值与速度值总和"：视图槽（含能力等级，2501 同口径）。
    const int sum = ctx->ws.getTempAbilityValue(self, NumericalPropertyIndex::SPECIAL_ATTACK)
        + ctx->ws.getTempAbilityValue(self, NumericalPropertyIndex::SPEED);
    const int pink = sum * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1186, effect_1186_def_pink_window)
SKILL_EFFECT(1163, effect_1163_panel_pink_growing)
