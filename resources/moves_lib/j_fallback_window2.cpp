// J 组 J167 · 未触发兜底窗族 —— 2 宏/21 载体（2026-10-04，worktree ../agents/j167）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1353 [rounds,chance,anomaly] "{0}回合内若自身先出手则{1}%令对手{2}，未触发则
//     消除对手回合类效果" —— 先手门（SKILL_EFFECT 域 acting_side_is_first，先短路
//     后掷骰）+ InRounds({0}) AFTER_ACTION 双桶（自身桶，出手侧才结算）+ 申报制
//     （apply_anomaly_checked）；未触发（j14_landed 三档取补：掷歪/免票都算）→
//     break_round_effects(对手)（J9 1200 直系：查免断+EVENT_BREAK+清回合类）。
//   1427 [rounds,chance,down] "{0}回合内每回合{1}%闪避对手攻击，未触发则受到的
//     伤害减少{2}%" —— 每回合 ROUND_START 掷点（首跳范式：施放当回合即掷、
//     2..{0} 走 NextRounds ROUND_START tick，J14 1024 切分记档）：
//     掷中 → ws.dodge_rate[自身]=1.0（当回合闪避攻击，ws 每回合 reset 天然收窗）；
//     掷歪 → 当回合 register_damage_effect(REDUCE_PCT, MITIGATE) 回调
//     d.final -= d.final*{2}/100（batch_c 417-429 同款，MITIGATE 不可被挡伤失效无视）。
//     ⚠️ 读法记档："每回合{1}%闪避"=每回合掷一次、命中则当回合全闪（roll=闪避态激活）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 10+11=21 全载体自洽、零剩余）：
//   1353 [rounds,chance,anomaly] ×10：[3,100,6]×4、[5,100,6]、[5,60,8]、[3,100,0]、
//     [4,80,22]、[4,100,13]、[2,100,30]——100% 确定档多（不绝影踔 26966 [3,100,6]）
//   1427 [rounds,chance,down] ×11：[3,50,50]×4、[3,30,30]×2、[3,80,50]、[3,70,50]、
//     [3,70,30]、[2,40,40]——全属性技武装载体
//     纯链：古渊飘渺 26516 [3,50,50]（50% 校准载体）
//
// ⚠️ 场景段偏离记档：看板行预留 2520-2529 与 J161（feat/j161）场景段**撞号**
//   （并行 author 会话分配竞态），本单场景改用 2680-2689（taskctl alloc 全局计数器
//   2026-10-04），看板行段号不回改——合并验收时以本记档为准（R 组/人工对账）。
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 2 id 无行为硬编码
//   ②custom_effect_overrides 零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J9 1200（break_round_effects）、J127 1617（申报未触发兜底）、
//   batch_c 417-429（REDUCE_PCT MITIGATE 回调）、J14 1024（首跳切分）。
// 场景回归：test/scenario/scenario_2680_fallback_window2.cpp（号段 2680-2689）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（j14_landed 同口径）。
bool j167_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 1353 tick —— 先手出手 → 申报异常；未触发 → 断对手回合 ──
EffectResult j167_1353_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    (void)args.int_args[4];
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // 本回合没出手
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    if (j167_landed(r)) {
        return EffectResult::kOk;
    }
    (void)g_core->break_round_effects(ctx, opp);   // 未触发 → 消除对手回合类
    return EffectResult::kOk;
}

// ── 1353 —— {0}回合内若自身先出手则{1}%令对手{2}，未触发则消除对手回合类 ──
EffectResult effect_1353_first_roll_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1353, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1353, args,
                     "1353 布局=[owner,target,rounds,chance,anomaly]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1353,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0, 1353, args,
                     "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1353, args,
                     "异常码越界（≤43）");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门（先短路后掷骰，SKILL_EFFECT 域）
    }
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1353, args.int_args[2],
                              {self, opp, args.int_args[3], args.int_args[4], 0},
                              &j167_1353_tick);
    }
    return EffectResult::kOk;
}

// ── 1427 回合内结算 —— 掷中闪避 / 掷歪减伤（当回合）──
void j167_1427_round(BattleContext* ctx, int self, int chance, int down) {
    if (roll_percent(chance)) {
        ctx->ws.dodge_rate[self] = 1.0f;   // 当回合闪避攻击（ws reset 天然收窗）
        return;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [self, down](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;
            }
            d.final -= d.final * down / 100;   // 受到的伤害 -{down}%（batch_c 同款整除）
        });
}

// ── 1427 ROUND_START tick ──
EffectResult j167_1427_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    j167_1427_round(ctx, args.int_args[0], args.int_args[2], args.int_args[3]);
    return EffectResult::kOk;
}

// ── 1427 —— {0}回合内每回合{1}%闪避，未触发则受伤-{2}% ──
EffectResult effect_1427_dodge_or_reduce_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1427, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1427, args,
                     "1427 布局=[owner,target,rounds,chance,down]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1427,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[4] > 0,
                     1427, args, "需要 回合/概率/减伤 > 0");
    // 首跳：施放当回合即掷（ROUND_START 已跑过，1024 切分记档）。
    j167_1427_round(ctx, self, args.int_args[3], args.int_args[4]);
    if (args.int_args[2] > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1427,
                                    args.int_args[2] - 1,
                                    {self, opp, args.int_args[3], args.int_args[4]},
                                    &j167_1427_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1353, effect_1353_first_roll_break);
SKILL_EFFECT(1427, effect_1427_dodge_or_reduce_window);
