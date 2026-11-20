// J97 · 对手攻后窗族 —— 1372×9 / 1614×6（2026-10-03，worktree ../agents/j97）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1372（args_num=3）"{0}回合内对手使用攻击技能后{1}%令对手{2}，未触发则消除对手
//   回合类效果"：
// 1614（args_num=4）"{0}回合内对手使用攻击技能后{1}%令对手{2}，未触发则令对手
//   全属性-{3}"：
//   - 窗 = InRounds({0}) 的对手侧 AFTER_ACTION tick（J43 767 同形两次注册：InRounds
//     含当回合；行动方过滤+used_skill+**selected_skill_is_attack** 三门——"对手使用
//     攻击技能后"）；
//   - 掷骰 = apply_anomaly_checked 申报制（{1}%，J3/J14 口径）；"未触发" = landed
//     三档取补（掷歪/免疫）→ 兜底分叉：
//     1372 = break_round_effects(对手)（180 直通原语，SUCCESS/NO_EFFECTS/IMMUNE 均无
//     后续子句）；1614 = 六槽逐项 stat_drop(对手, i, {3}, 自)（938 循环，查免弱）。
//   - 实例：银光护盾 27203 [1239,1372,1475]|[1,2,3,100,5,3,50]（1239/1475 自侧票
//     良性已注册）；风泽中孚 27716 [191,1614,1469]|[4,4,100,7,1,3,30]（1469 自侧良性）。
//   ⚠️ 选载体避同链 922 反馈窗（天佑勇者族）：其 ROUND_END 清自身下降并等量反馈
//     会把兜底 stat_drop 在回合末清掉（实测六槽归零），场景与实现均不可用此链。
//
// 动工前置三查（2026-10-03）：①内核 grep 1372/1614 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1372=3/1614=4 与 effect_info.args_num 一致；15 载体切片自洽。
// 先例：J43 767（InRounds 对手侧窗形）、J70（兜底分叉）、J24 153（TAKE_DAMAGE 面）、
//   180（break_round_effects 直通）、938（全属性降循环）。
// 场景回归：test/scenario/scenario_1870_foe_attack_window.cpp（1870 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// AFTER_ACTION 桶的当前行动方（J12 同款小工具）。
int j97_bucket_acting_side(const BattleContext* ctx) {
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
bool j97_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 窗 tick：对手用攻击技后 → {1}%令对手{2}；未触发 → 兜底。
// payload = [owner, opp, rounds, chance, anomaly, kind, fallback]
// （kind 0=1372 断回合 / 1=1614 全属性降）。
EffectResult j97_foe_attack_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !a.int_args || a.int_count < 7) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[3];
    const int anomaly = a.int_args[4];
    const int kind = a.int_args[5];
    const int fallback = a.int_args[6];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // 三门：对手行动方 + 真用技 + 攻击技（"对手使用攻击技能后"）。
    if (j97_bucket_acting_side(c) != opp || !used_skill_this_round(c, opp)
        || !selected_skill_is_attack(c, opp)) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (chance > 0 && is_valid_abnormal_status_id(anomaly)
        && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
        landed = j97_landed(r);
    }
    if (landed) {
        return EffectResult::kOk;
    }
    // 未触发兜底
    if (kind == 0) {
        if (g_core->break_round_effects) {
            (void)g_core->break_round_effects(c, opp);
        }
    } else {
        if (g_core->stat_drop && fallback > 0) {
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                const StatDropResult r = g_core->stat_drop(c, opp, i, fallback, self);
                std::fprintf(stderr, "[j97-dbg] stat_drop slot%d r=%d lvl_after=%d\n", i,
                             static_cast<int>(r), c->ability_levels[opp][i]);
            }
        }
    }
    return EffectResult::kOk;
}

// 共用武装：InRounds({0}) 对手侧 AFTER_ACTION 两次注册（767 同形，含当回合）。
void j97_arm_window(BattleContext* ctx, int effect_id, int owner, const EffectArgs& args) {
    std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    payload.push_back(effect_id == 1372 ? 0 : 1);
    if (effect_id == 1372) {
        payload.push_back(0);   // 1372 无 {3} 降幅参——补位对齐 7 元布局
    }
    // ⚠️ 挂 **对手** 名下（E10 703 对敌口径）：触发点=对手行动结束，注册到对手侧
    // 才会在对手 pass 里执行（挂自身名下永远轮不到——J97 实测勘误）。
    const int opp = args.int_args[1];
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, opp, effect_id, /*rounds=*/args.int_args[2],
                              payload, &j97_foe_attack_tick);
    }
}

}  // namespace

// ── 1372 —— {0}回合内对手使用攻击技能后{1}%令对手{2}，未触发→消除对手回合类 ──
// int_args = [owner, target, rounds, chance, anomaly]（args_num=3）。
EffectResult effect_1372_foe_attack_window_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1372, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1372, args,
                     "1372 布局=[owner,target,rounds,chance,anomaly]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1372, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0, 1372, args,
                     "需要 回合/概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1372, args,
                     "异常码越界（≤43）");
    j97_arm_window(ctx, 1372, owner, args);
    return EffectResult::kOk;
}

// ── 1614 —— 同门，未触发→对手全属性-{3} ──
// int_args = [owner, target, rounds, chance, anomaly, all_down]（args_num=4）。
EffectResult effect_1614_foe_attack_window_stats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1614, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1614, args,
                     "1614 布局=[owner,target,rounds,chance,anomaly,all_down]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1614, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0 && args.int_args[5] > 0,
                     1614, args, "需要 回合/概率/降幅 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(args.int_args[4]), 1614, args,
                     "异常码越界（≤43）");
    j97_arm_window(ctx, 1614, owner, args);
    return EffectResult::kOk;
}

SKILL_EFFECT(1372, effect_1372_foe_attack_window_break)
SKILL_EFFECT(1614, effect_1614_foe_attack_window_stats)
