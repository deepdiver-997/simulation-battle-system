// J70 · 每回合窗族 —— 1181×14 / 1766×4（2026-10-03，worktree ../agents/j70）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1181（args_num=4）"{0}回合内每回合使用技能{1}%令对手{2}，未触发则令对手全属性-{3}"：
// 1766（args_num=4）"{0}回合内每回合使用技能{1}%令对手{2}，未触发则令对手所有技能
//   PP值-{3}"：
//   - 窗 = NextRounds({0}) 的 AFTER_ACTION 双侧探针（每回合一次）；起点 R+1——载体
//     当次用技不掷（"每回合"从下回合起算，J14 850 首跳切分同精神）；
//   - tick 判 = 自身 acting（桶 per-owner 天然保证）+ 真用技（used_skill_this_round，
//     J6 口径：嗑药/空转回合不掷）→ apply_anomaly_checked 申报制（J3/J14 口径，
//     {1}% 概率闸门管道管辖）；
//   - "未触发" = 概率掷歪 ∫ 免疫/抵抗（ApplyAnomalyResult 非 SUCCESS/REPLACED_EXISTING/
//     DURATION_EXTENDED 三档，j14_landed 同口径）→ 兜底：
//     1181 = 全属性-{3}（六槽逐项 stat_drop 查免弱，938 同款循环）；
//     1766 = pp_reduce 全槽 -{3}（原语逐槽钳 0 + 逐槽事件，999 归零同通道）。
//   - 实例：恐怖之潮 [191,47,1181]|[4,4,3,100,6,1]（47=自身免弱票，良性）、
//     踏雪荧辉 27829 [191,1766,843]|[4,3,100,5,2,2,2]（843=自身先制窗，断言面外）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1181/1766 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count 1181=4/
//   1766=4 与 effect_info.args_num 一致；18 载体链切片自洽（[回合,概率,异常码,降幅]）。
// 先例：E10 703（对手/自身侧 AFTER_ACTION 窗）、J14（申报制 + landed 三档）、
//   938（全属性降循环）、pp_zero_all（common.h，g_core->pp_reduce 通道）。
// 场景回归：test/scenario/scenario_1570_roundly_window.cpp（1570 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// AFTER_ACTION 桶的当前行动方（J12/J41 同款小工具）。
int j70_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// 申报制"落地"三档（j14_landed 同口径）。
bool j70_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 窗 tick：自身每回合用技 → {1}%令对手{2}，未触发 → 兜底 ──
// payload = [owner, opp, rounds, chance, anomaly, fallback, kind, arm_round]
// （kind 0=全属性降兜底 / 1=PP 减兜底；arm_round=武装回合，防同回合早执行——
//   AFTER_ACTION 桶晚于 SKILL_EFFECT 武装点，NextRounds 起点挡不住当回合，J61 878 同款）
EffectResult j70_window_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !a.int_args || a.int_count < 8) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[3];
    const int anomaly = a.int_args[4];
    const int fallback = a.int_args[5];
    const int kind = a.int_args[6];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (c->roundCount <= a.int_args[7]) {
        return EffectResult::kOk;   // 武装当回合不掷（"每回合"从下回合起）
    }
    if (j70_bucket_acting_side(c) != self || !used_skill_this_round(c, self)) {
        return EffectResult::kOk;   // 每回合用技才掷（嗑药/空转不算，J6 口径）
    }
    bool landed = false;
    if (chance > 0 && is_valid_abnormal_status_id(anomaly) && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
        landed = j70_landed(r);
    }
    if (landed) {
        return EffectResult::kOk;
    }
    // 未触发兜底
    if (kind == 0) {
        if (!g_core->stat_drop || fallback <= 0) {
            return EffectResult::kOk;
        }
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            (void)g_core->stat_drop(c, opp, i, fallback, self);
        }
    } else {
        if (!g_core->pp_reduce || fallback <= 0) {
            return EffectResult::kOk;
        }
        (void)g_core->pp_reduce(c, opp, fallback);
    }
    return EffectResult::kOk;
}

// 共用武装：NextRounds({0}) AFTER_ACTION 双侧探针（起点 R+1，载体当次不掷）。
// payload 尾追 kind 位（0=全属性降兜底 / 1=PP 减兜底），tick 按它分叉。
void j70_arm_window(BattleContext* ctx, int effect_id, int owner, const EffectArgs& args) {
    std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    payload.push_back(effect_id == 1181 ? 0 : 1);
    payload.push_back(ctx->roundCount);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(ctx, st, owner, effect_id, /*rounds=*/args.int_args[2],
                                    payload, &j70_window_tick);
    }
}

}  // namespace

// ── 1181 —— {0}回合内每回合用技{1}%令对手{2}，未触发→对手全属性-{3} ──
// int_args = [owner, target, rounds, chance, anomaly, fallback, kind=0]（args_num=4）。
EffectResult effect_1181_roundly_window_stats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1181, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1181, args,
                     "1181 布局=[owner,target,rounds,chance,anomaly,fallback]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1181, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[5] > 0,
                     1181, args, "需要 rounds/chance/fallback > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1181, args,
                     "异常码越界（≤43）");
    j70_arm_window(ctx, 1181, owner, args);
    return EffectResult::kOk;
}

// ── 1766 —— 同门，未触发→对手所有技能 PP-{3} ──
EffectResult effect_1766_roundly_window_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1766, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1766, args,
                     "1766 布局=[owner,target,rounds,chance,anomaly,fallback]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1766, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[5] > 0,
                     1766, args, "需要 rounds/chance/fallback > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1766, args,
                     "异常码越界（≤43）");
    j70_arm_window(ctx, 1766, owner, args);
    return EffectResult::kOk;
}

SKILL_EFFECT(1181, effect_1181_roundly_window_stats)
SKILL_EFFECT(1766, effect_1766_roundly_window_pp)
