// J107 · 申报制未触发/被击败清强化族 —— 1003×14 / 197×14（2026-10-03，worktree ../agents/j107）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1003（args_num=3）"命中后{0}%使对手{1}，未触发则使对手全属性-{2}"：
//   - SKILL_EFFECT 直通：apply_anomaly_checked 申报制（{0}%，J3/J14 口径）；
//     "未触发" = landed 三档取补（J70 同款）→ 六槽逐项 stat_drop(对手, {2})（938 循环）。
//   - 实例：极浪奇天咒 30354 [80,15,1]、恒温熔解 37736 [80,2,1]（80% 档——场景走
//     免疫确定性兜底 + 统计侧证双保险）。
// 197（args_num=0）"{0}回合内若被对方击败，则对手所有能力加强状态消失"：
//   - EVENT_DEATH watcher（once=true="被击败"接棒点，ev.target==自身，K3 330/J81 1319
//     事件面直系）→ clear_stat_boosts(对手)（对手强化清零，J1 1683 反向面）；
//   - 零参 → bind 补位；切片尾参 [2]/[3] 未绑定（参差记档）。
//   - 实例：眩光之阵 26115 [197]|[2]、芙蓉清浅 33700 [197]|[3]（属性技纯链）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1003/197 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1003=3/197=0 与 effect_info.args_num 一致；28 载体切片自洽。
// 先例：J70（兜底分叉）、938（全属性降循环）、K3/J81（EVENT_DEATH watcher）、
//   J16 1109（clear_stat_boosts 事件面）。
// 场景回归：test/scenario/scenario_2010_apply_fallback_death_clear.cpp（2010 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <memory>

namespace {

// 申报制"落地"三档（j14_landed 同口径）。
bool j107_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

}  // namespace

// ── 1003 —— 命中后{0}%使对手{1}，未触发则使对手全属性-{2} ──
// int_args = [owner, target, chance, anomaly, all_down]（args_num=3）。
EffectResult effect_1003_chance_fallback_all_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1003, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1003, args,
                     "1003 布局=[owner,target,chance,anomaly,all_down]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int all_down = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 1003, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && all_down > 0, 1003, args, "需要 概率/降幅 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1003, args, "异常码越界（≤43）");
    if (!g_core || !g_core->apply_anomaly_checked || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, target, anomaly, random_anomaly_duration(), self, chance,
            ChanceSource::Skill);
        landed = j107_landed(r);
    }
    if (landed) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, target, i, all_down, self);
    }
    return EffectResult::kOk;
}

// ── 197 —— 若被对方击败，则对手所有能力加强状态消失 ──
// int_args = [owner, target]（args_num=0，bind 补位）。
EffectResult effect_197_death_clear_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 197, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 197, args,
                     "197 布局=[owner,target]（零参补位）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 197, args, "owner 越界");
    if (!g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->clear_stat_boosts) {
                return;
            }
            (void)g_core->clear_stat_boosts(c, 1 - self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

SKILL_EFFECT(1003, effect_1003_chance_fallback_all_down)
SKILL_EFFECT(197, effect_197_death_clear_boosts)
