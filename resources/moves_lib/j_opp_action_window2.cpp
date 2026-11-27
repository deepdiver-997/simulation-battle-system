#include <utils/sbs_rand.h>
// J 组 J160 · 对手行为窗族二 —— 2 宏/20 载体（2026-10-04，worktree ../agents/j160）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1118 [rounds,chance,anomaly,down]
//     "{0}回合内对手若使用攻击技能则{1}%{2}，若使用属性技能则全属性-{3}"
//     —— 对手行为二分窗（J97 口径：窗条目挂**对手名下**，对手 after-action 结算）：
//     攻击技分支 roll_percent({1}) → apply_anomaly_checked(对手,{2})（申报制）；
//     属性技分支 → 对手六槽（含命中）逐项 stat_drop {3} 级（749/938 同款、各查免弱）。
//     行为分类 = executed_skill_this_round + executed_skill_is_attack（判据统一
//     §四点五：ws.skill_type_view 实际物化类型，替换技/变型技正确；先分类后掷骰，
//     属性技分支不耗 rand）。
//   1089 [rounds,kinds] "{0}回合内对手使用攻击技能后随机附加给对手{1}种异常状态"
//     —— 同窗攻击技分支：**随机不重复**抽 {1} 个官方异常码（0..9）逐个申报
//     （apply_anomaly_checked chance=100——模板无概率字样；落地由异常系统幂等）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 10+10=20 全载体自洽、零剩余）：
//   1118 [rounds,chance,anomaly,down] ×10：全 [3,100,X,1]（主干确定档）
//     纯链：废油瓦斯 37271 [3,100,10,1]（特140，码 10）
//   1089 [rounds,kinds] ×10：[3,1]×3、[3,2]×5、[4,1]×1、[3,2]…全属性技载体
//     （伴生全已实现；场景取 大海守望 25616 [3,1] / 灵魂绝响 25287 [3,2]，
//     异常**计数**断言免种子校准）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 2 id 无行为硬编码
//   ②custom_effect_overrides 零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J97 1372（对手行为窗挂对手名下）、判据统一 §四点五（executed_* 分类）、
//   749/938（全属性逐槽 stat_drop）、J3 申报制。
// 场景回归：test/scenario/scenario_2510_opp_action_window2.cpp（号段 2510-2519）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 对手行为二分 tick：攻击技→概率异常；属性技→全属性降（1118 专用载荷）。
EffectResult j160_1118_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core
        || !g_core->stat_drop || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];       // 窗主（施放方）
    const int opp = args.int_args[1];        // 行为方（窗条目挂其名下）
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int down = args.int_args[4];
    (void)self;
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;   // 对手本回合没出手（被控/NONE）
    }
    if (!executed_skill_is_attack(ctx, opp)) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            (void)g_core->stat_drop(ctx, opp, i, down, 1 - opp);   // 全属性-{down}
        }
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                        1 - opp, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 1118 —— 对手用攻击技则{1}%{2}，用属性技则全属性-{3}（{0} 回合窗）──
EffectResult effect_1118_opp_action_branch_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1118, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1118, args,
                     "1118 布局=[owner,target,rounds,chance,anomaly,down]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1118,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[5] > 0,
                     1118, args, "需要 回合/概率/降级 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1118, args,
                     "异常码越界（≤43）");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, opp, 1118, args.int_args[2],
                              {self, opp, args.int_args[3], args.int_args[4],
                               args.int_args[5], 0},
                              &j160_1118_tick);
    }
    return EffectResult::kOk;
}

// 随机不重复抽 n 个官方异常码 0..9 逐个申报（1089）。
void j160_random_anomalies(BattleContext* ctx, int target, int actor, int n) {
    if (!g_core || !g_core->apply_anomaly_checked || n <= 0) {
        return;
    }
    int picked[10] = {};
    int count = 0;
    int guard = 0;
    while (count < n && guard < 200) {   // 不重复抽 0..9；guard 防死循环（n≤10）
        ++guard;
        const int code = ctx->rng().next() % 10;
        if (picked[code]) {
            continue;
        }
        picked[code] = 1;
        ++count;
        (void)g_core->apply_anomaly_checked(ctx, target, code, random_anomaly_duration(),
                                            actor, 100, ChanceSource::Skill);
    }
}

// ── 1089 tick —— 对手用攻击技后随机附加 {kinds} 种异常 ──
EffectResult j160_1089_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int kinds = args.int_args[2];
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;
    }
    if (!executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 属性技无后缀（模板只写"使用攻击技能后"）
    }
    j160_random_anomalies(ctx, opp, 1 - opp, kinds);
    return EffectResult::kOk;
}

// ── 1089 —— {0}回合内对手使用攻击技能后随机附加给对手{1}种异常 ──
EffectResult effect_1089_opp_attack_random_anomalies(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1089, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1089, args,
                     "1089 布局=[owner,target,rounds,kinds]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1089,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[3] <= 10,
                     1089, args, "需要 回合 > 0 且 种数 ∈ [1,10]");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, opp, 1089, args.int_args[2],
                              {self, opp, args.int_args[3], 0},
                              &j160_1089_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1118, effect_1118_opp_action_branch_window);
SKILL_EFFECT(1089, effect_1089_opp_attack_random_anomalies);
