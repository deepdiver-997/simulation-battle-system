// J 组 J75 · 消双方回合族 —— 1414×17 / 1139×17（2026-10-03，worktree ../agents/j75）
//
// 家族语义（effect_info 官方模板即权威）：
//   1414（arg_count=1）"消除双方回合类效果，消除成功则双方{0}"
//   1139（arg_count=3）"消除双方回合类效果并为双方附加{0}回合{1}/{2}，自身{2}状态解除后则恢复自身全部体力"
//
// 实现口径：
//   - 共门 = break_round_effects 双侧直呼（1032/J1 直系原语；IMMUNE/INVALID 返回值
//     不抛错、按未消处理）。
//   - 1414 = 任一侧 SUCCESS 即"消除成功" → 双方各施加 {0}（申报制 chance=100、
//     random_anomaly_duration——模板无时长字样，2038 的 a_opp 同款）。
//   - 1139 = 双侧消**无门槛**（模板"并"字直通）→ 双侧附加：对手 {1}、自身 {2}，
//     各 {0} 回合**指定时长**（573/J44 547 同款，非 random_duration）→ 自身 {2}
//     状态解除（自然到期路径 EVENT_ANOMALY_EXPIRED，2038 星盘族口径——效果解除/
//     转移不发，偏差记档）→ heal_max_hp_fraction(1) 回满（内部钳满）。
//
// 载体切片（unity 库 json_each 精确匹配 17/17=34；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1414 [异常码]：{30}×3/{2}×3/{16}×2/{36}/{32}/…（纯链载体 血染战场 35054 [16] 特140）
//   1139 [回合,对手码,自身码]：{3,2,2}×5/{3,16,16}×2/{2,1,1}/…
//     （纯链载体 龙契·蛊 37739 [2,1,1] 特110）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1414/1139 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1032/2038（break_round_effects 门与双侧消、EVENT_ANOMALY_EXPIRED watcher，
// J1）、547（apply_anomaly_checked 指定时长双侧，J44）、1255（deal_pink 申报制）。
// 场景回归：test/scenario/scenario_1650_break_both_rounds.cpp（1650 号段，一文件两宏）。

#include "common.h"

namespace {

// 双侧消回合类效果；任一侧 SUCCESS 返回真（1414 的"消除成功"门）。
bool j75_break_both_rounds(BattleContext* ctx, int owner, int opp) {
    if (!g_core || !g_core->break_round_effects) {
        return false;
    }
    const bool opp_broken =
        g_core->break_round_effects(ctx, opp) == BreakResult::SUCCESS;
    const bool self_broken =
        g_core->break_round_effects(ctx, owner) == BreakResult::SUCCESS;
    return opp_broken || self_broken;
}

// 单侧申报制异常（chance=100；该侧自身免疫票照挡）。
void j75_apply_one(BattleContext* ctx, int actor, int target, int anomaly, int duration) {
    if (!g_core || !g_core->apply_anomaly_checked
        || !is_valid_abnormal_status_id(anomaly)) {
        return;
    }
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, duration, actor,
                                        /*chance=*/100, ChanceSource::Skill);
}

// ── 1414 —— 消除双方回合类效果，消除成功则双方{0} ──
// int_args = [owner, target, anomaly]（args_num=1）。
EffectResult effect_1414_break_both_then_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1414, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1414, args,
                     "1414 布局=[owner,target,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1414, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1414, args,
                     "需要 异常码 ∈ [0,43]");
    if (!j75_break_both_rounds(ctx, owner, opp)) {
        return EffectResult::kOk;   // "消除成功"门槛（双方都无回合类效果/免断 → 不触发）
    }
    const int duration = random_anomaly_duration();
    j75_apply_one(ctx, owner, opp, anomaly, duration);
    j75_apply_one(ctx, owner, owner, anomaly, duration);
    return EffectResult::kOk;
}

// ── 1139 —— 消除双方回合类效果并为双方附加{0}回合{1}/{2}，自身{2}状态解除后则恢复自身全部体力 ──
// int_args = [owner, target, rounds, a_opp, a_self]（args_num=3）。
EffectResult effect_1139_break_both_dual_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1139, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1139, args,
                     "1139 布局=[owner,target,回合,对手码,自身码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int a_opp = args.int_args[3];
    const int a_self = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1139, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1139, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(a_opp) && is_valid_abnormal_status_id(a_self),
                     1139, args, "需要 异常码 ∈ [0,43]");
    (void)j75_break_both_rounds(ctx, owner, opp);   // "并"字直通：无消除成功门槛
    // 双侧附加：对手 {1} / 自身 {2}，各 {0} 回合指定时长（547 同款，非 random_duration）。
    j75_apply_one(ctx, owner, opp, a_opp, rounds);
    j75_apply_one(ctx, owner, owner, a_self, rounds);
    // 自身 {2} 解除（自然到期）→ 回满（2038 星盘族口径；once、长窗 9 回合）。
    const int r0 = ctx->roundCount;
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_EXPIRED, owner, r0, /*duration_rounds=*/9,
        /*once=*/true,
        [owner, a_self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.amount != a_self || !g_core
                || !g_core->heal_amount) {
                return;
            }
            (void)heal_max_hp_fraction(c, owner, 1);   // 恢复全部（内部钳满）
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1414, effect_1414_break_both_then_anomaly)
SKILL_EFFECT(1139, effect_1139_break_both_dual_anomaly)
