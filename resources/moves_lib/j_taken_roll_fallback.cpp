// J85 · 受击掷骰族 —— 1195×15（2026-10-03，worktree ../agents/j85）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1195（args_num=4）"{0}回合内自身受到攻击{1}%使对手{2}，若未触发则对手受到{3}点
//   固定伤害"：
//   - 窗 = InRounds({0}) EVENT_TAKE_DAMAGE 自侧 watcher（J24 153 事件面：
//     ev.target==self、NORMAL 红伤、amount>0）；
//   - 掷骰 = apply_anomaly_checked 申报制（{1}%，J3/J14 口径）；"未触发" = 非
//     SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED 三档（j14_landed 取补——掷歪/
//     免疫都算）→ fixed_damage(对手, {3})（J70 兜底分叉同款）。
//   - 实例：极速幻影 25545 [1194,1195,1017]|[4,4,100,19,300]（属性技；1194=反转
//     自身下降+免降窗，已注册 canglan——空场无下降反转失败=良性）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1195 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1195=4 与 effect_info.args_num 一致；15 载体切片自洽（[回合,概率,异常码,固伤]）。
// 先例：J24 153（TAKE_DAMAGE watcher）、J14（申报制 landed 三档）、J70（兜底分叉）。
// 场景回归：test/scenario/scenario_1750_taken_roll_fallback.cpp（1750 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（j14_landed 同口径；匿名空间不跨 TU，就地重写）。
bool j85_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

}  // namespace

// ── 1195 —— {0}回合内自身受到攻击{1}%使对手{2}，未触发→对手受到{3}点固伤 ──
// int_args = [owner, target, rounds, chance, anomaly, fixed]（args_num=4）。
EffectResult effect_1195_taken_roll_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1195, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1195, args,
                     "1195 布局=[owner,target,rounds,chance,anomaly,fixed]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int fixed = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1195, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && fixed > 0, 1195, args,
                     "需要 回合/概率/固伤 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1195, args, "异常码越界（≤43）");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, chance, anomaly, fixed](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly_checked
                || !g_core->fixed_damage) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "受到攻击" = 红伤
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, chance,
                ChanceSource::Skill);
            if (j85_landed(r)) {
                return;
            }
            (void)g_core->fixed_damage(c, opp, fixed);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

SKILL_EFFECT(1195, effect_1195_taken_roll_fallback)
