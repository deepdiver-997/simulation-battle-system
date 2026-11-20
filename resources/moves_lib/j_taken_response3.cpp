// J 组 J127 · 受击响应族三 —— 2 宏/25 载体（2026-10-04，worktree ../agents/j127）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1085 [rounds,阈值,固伤] "{0}回合内受到的伤害低于{1}则附加{2}点固定伤害"
//     —— InRounds({0}) EVENT_TAKE_DAMAGE 自侧 watcher（J85 1195 通道）+ 低伤量判：
//     ev.damage_kind==NORMAL（攻击技门）、ev.amount>0 且**严格 <** {1} →
//     fixed_damage(ev.actor, {2})（反打攻击者——ev.actor 即本次红伤来源方）。
//     量判读 ev.amount=实扣量（J15 775 同判据通道：TAKE_DAMAGE 在扣血后发）；
//     窗内每次合格受击各判一次（1195 非一次性同款）。阈值 300 vs atk=1 裸伤 1~3 /
//     atk=3000 裸伤 ≥858——浮动带远离阈值，场景闭式确定性。
//   1617 [rounds,概率,异常码,提升] "{0}回合内受到攻击后{1}%使对手{2}，未触发则自身
//     全属性+{3}" —— 1195 直系换兜底：同一 watcher 面 → apply_anomaly_checked
//     申报制（{1}%，J3/J14 口径）；"未触发" = 非 SUCCESS/REPLACED_EXISTING/
//     DURATION_EXTENDED 三档（j14_landed 口径）→ all_stats_up(自身六槽, +{3})
//     （J8/J82 直系 + J107 兜底形反转向：1195 兜底打对手固伤、本条自升）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 13+12=25 全载体自洽、零剩余）：
//   1085 ×13：[3,300,300]×10、[3,300,200]×2、[4,300,300]×1
//     链短者：黑色战殇 25275（48/47 已实现伴生=自身强化/免异常窗，对受击断言良性）、
//     齐力同心 28263（126/1925 ✓）
//   1617 ×12：概率 70/100；异常码 2/6/7/9/15/22/0；提升恒 1
//     纯链：窃笑潜逃 36981 [3,70,15,1]（物攻130，掷点校准载体）
//     ⚠️ 走魂之术 27316 链带 874（J116 在产）——工单行裁定弃用，本单不选
//     ⚠️ 食腐心得 28697 异常码 0（麻痹）：apply_anomaly_checked 现代通道非确定
//     （速查 §实用技巧），场景不选
//
// 动工前置三查（2026-10-04 复核）：①内核 grep 1085/1617 无硬编码（无 CARRY_EFFECT
//   牵连）②custom_effect_overrides 2 id 零行 ③moves_lib 无既有注册——真缺口。
// 先例：J85 1195（受击 watcher+申报制+兜底分叉）、J15 775（InRounds 起点折算+
//   实扣量判）、J14（landed 三档）、J42 989（低伤阈值形——攻方 DETECT 位，方向相反）。
// 场景回归：test/scenario/scenario_2210_taken_response3.cpp（号段 2210-2219）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（j14_landed 同口径；匿名空间不跨 TU，就地重写）。
bool j127_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// ── 1085 —— {0}回合内受到的伤害低于{1}则附加{2}点固定伤害（反打攻击者）──
// int_args = [owner, target, rounds, threshold, fixed]（args_num=3）。
EffectResult effect_1085_low_taken_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1085, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1085, args,
                     "1085 布局=[owner,target,回合,阈值,固伤]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    const int fixed = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1085, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0 && fixed > 0, 1085, args,
                     "需要 回合/阈值/固伤 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    // 窗口 InRounds 含当回合（775 同款起点折算，后手不少算）；跨回合 watcher TEAM scope。
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, start, rounds, /*once=*/false,
        [self, threshold, fixed](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "受到攻击" = 红伤（1195 同门）
            }
            if (ev.actor < 0 || ev.actor > 1 || ev.actor == self) {
                return;   // 反打对象=攻击者
            }
            if (ev.amount >= threshold) {
                return;   // 严格低于阈值才附加
            }
            (void)g_core->fixed_damage(c, ev.actor, fixed);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1617 —— {0}回合内受到攻击后{1}%使对手{2}，未触发则自身全属性+{3} ──
// int_args = [owner, target, rounds, chance, anomaly, up]（args_num=4）。
EffectResult effect_1617_taken_roll_self_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1617, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1617, args,
                     "1617 布局=[owner,target,回合,概率,异常码,提升]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int up = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1617,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && up > 0, 1617, args,
                     "需要 回合/概率/提升 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1617, args, "异常码越界（≤43）");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, start, rounds, /*once=*/false,
        [self, opp, chance, anomaly, up](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "受到攻击" = 红伤（1195 同门）
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, chance,
                ChanceSource::Skill);
            if (j127_landed(r)) {
                return;
            }
            all_stats_up(c, self, up);   // 未触发兜底：自身六槽（含命中）+{3}
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1085, effect_1085_low_taken_fixed);
SKILL_EFFECT(1617, effect_1617_taken_roll_self_up);
