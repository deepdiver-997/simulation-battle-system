#include <utils/sbs_rand.h>
// J 组 J161 · 窗内掷骰异常族 —— 2 宏/22 载体（2026-10-04，worktree ../agents/j161）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04——⚠️ 工单行"纯链 [3,100]"
// 系 771 切片（2 参），1007 全 11 载体 4 参 [rounds,chance,anomaly,down] 自洽）：
//   1007 [rounds,chance,anomaly,down] "{0}回合内每次攻击都有{1}%概率令对手{2}，
//     未触发则对手全属性-{3}" —— 施放方攻击窗（载体全为属性技=武装技）：自身
//     AFTER_ACTION 双桶 + executed_skill_this_round/executed_skill_is_attack 门
//     （判据统一 §四点五："每次攻击"=成功使用攻击技；used_skill 旧门不沿用）→
//     apply_anomaly_checked 申报制（roll 在闸内）；未触发（j14_landed 三档取补）
//     → 对手六槽（含命中）逐项 stat_drop {3} 级（749/938 同款）。
//   771 [rounds,chance] "{0}回合内使用攻击技能{1}%随机附加1种异常状态"
//     —— 同窗同门：roll_percent({1}) 命中 → rand 抽官方异常码 0..9 一种 →
//     apply_anomaly_checked（chance=100 申报；抽码与闸门两次随机并存记档）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 11+11=22 全载体自洽、零剩余）：
//   1007 ×11：[3,70,6,1]×4、[3,50,6,1]×2、[3,100,5/6,1]×2、[3,80,27,1]、
//     [3,70,15,1]、[3,70,0,1]——全属性技武装载体
//     短链：泰坦威慑 25993 [3,50,6,1]（伴生 843=恢复，良性）
//   771 ×11：[3,50]×3、[3,80]×2、[3,100]×4、[3,50]/[3,80]…
//     纯链：花语呢喃 28479 [3,100]、石之无畏 28482 [3,100]（100% 确定档）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 2 id 无行为硬编码
//   ②custom_effect_overrides 零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J6 597 形（AFTER_ACTION 双桶窗）、判据统一 §四点五（executed_* 门）、
//   J127 1617（申报制未触发→全属性降兜底）、J160 1089（rand 抽码 0..9）。
// 场景回归：test/scenario/scenario_2520_window_roll_anomaly.cpp（号段 2520-2529）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（j14_landed 同口径）。
bool j161_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 1007 tick —— 自身攻击 → 申报异常；未触发 → 对手全属性降 ──
EffectResult j161_1007_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core
        || !g_core->apply_anomaly_checked || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int down = args.int_args[4];
    (void)args.int_args[5];
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // 本回合没用技（被控/NONE）
    }
    if (!executed_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;   // "每次攻击"=攻击技
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    if (j161_landed(r)) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, down, self);   // 未触发 → 全属性-{down}
    }
    return EffectResult::kOk;
}

// ── 1007 —— {0}回合内每次攻击{1}%令对手{2}，未触发则对手全属性-{3} ──
EffectResult effect_1007_attack_roll_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1007, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1007, args,
                     "1007 布局=[owner,target,rounds,chance,anomaly,down]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1007,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[5] > 0,
                     1007, args, "需要 回合/概率/降级 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1007, args,
                     "异常码越界（≤43）");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1007, args.int_args[2],
                              {self, opp, args.int_args[3], args.int_args[4],
                               args.int_args[5], 0},
                              &j161_1007_tick);
    }
    return EffectResult::kOk;
}

// ── 771 tick —— 自身攻击 → 概率随机 1 种异常（0..9）──
EffectResult j161_771_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    (void)args.int_args[3];
    if (!executed_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (!executed_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;
    }
    const int code = sbs_rand() % 10;   // 官方异常码 0..9 随机一种（J160 1089 同抽法）
    (void)g_core->apply_anomaly_checked(ctx, opp, code, random_anomaly_duration(), self,
                                        chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 771 —— {0}回合内使用攻击技能{1}%随机附加1种异常状态 ──
EffectResult effect_771_attack_roll_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 771, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 771, args,
                     "771 布局=[owner,target,rounds,chance]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 771,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0, 771, args,
                     "需要 回合/概率 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 771, args.int_args[2],
                              {self, opp, args.int_args[3], 0}, &j161_771_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1007, effect_1007_attack_roll_drop);
SKILL_EFFECT(771, effect_771_attack_roll_random_anomaly);
