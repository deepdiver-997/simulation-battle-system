// J41 · 后手先制/免致命/miss惩罚族 —— 1549×20 / 662×19 / 648×19（2026-10-02，worktree ../agents/j41）
//
// 家族语义（effect_info 官方模板），三条都与"出手结果条件"相关：
//
// 1549（arg_count=1）"后出手时下回合攻击技能先制+{0}"：
//   - AFTER_ACTION 探针（897 桥形）："后出手"= 行动方是自身且自身**非先手方**
//     （preemptive_right 推 first 侧，acting==self && acting!=first）；后缀 =
//     NextRounds(1) 的 MOVE_RIGHT tick：selected_skill_is_attack 门（"攻击技能"限定，
//     681 同款）→ ws.preemptive_level[自身] += {0}（843/499 通道）。
//   - 实例：入鞘 28227 [1]（全库 20 载体，{0}∈{1,2}）。
// 662（arg_count=1）"{0}回合内免疫致命一击伤害"：
//   - grant_immunity(INSTANT_KILL, InRounds {0} 回合，counts=0 窗口票）——消费侧
//     force_hp_to_zero 的免票门（495 家秒杀体系）已存在，纯授予。
//   - 实例：不坏龙玉 23543 [3]（全库 19 载体，{0}∈{2,3}）。
// 648（arg_count=1）"若对手下次使用的攻击技能未命中则令对手{0}"：
//   - EVENT_SKILL_INVALID watcher（2300 同款常驻 + storage 句柄重绑——"下次"每次
//     用技重新武装）+ fired 旗一次性；触发过滤 = ev.actor==对手 && roundChoice 仍在
//     （selected_skill_is_attack(对手)——"攻击技能"限定；⚠️ INVALID 事件 miss/封属
//     合并发射（query_usage 两档同一 emit），无法后验拆分 → 宽口径记档）；
//   - 后缀 = apply_anomaly_checked(对手, {0}, 100)（{0}=异常码，全载 {2,5,6,8}）。
//   - 实例：惊魂飞天断 16542 [5]（全库 19 载体）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1549/662/648 无行为硬编码（INSTANT_KILL
//   票类型与 SKILL_INVALID 事件均为通用设施）②effect_meta 无分类牵连 ③moves_lib
//   （本分支）无既有注册——真缺口。载体数核对（EXISTS）：20/19/19；全量切片核验
//   58 载体布局零异常（1549 [prio] / 662 [rounds] / 648 [异常码]）。
// 先例：843/499（MOVE_RIGHT 先制）、681（selected_skill_is_attack 门）、1194
//   （grant_immunity 票形）、2300（常驻 watcher + storage 句柄重绑）、464（申报制）、
//   场景 001（条件先制翻盘法）、1235（dodge_rate 直写造 miss）。
// 场景回归：test/scenario/scenario_1260_second_preempt.cpp（1260 号段，一文件三宏）。

#include "common.h"

#include <any>

#include <abnormal-system/abnormal-types.h>

namespace {

// AFTER_ACTION 桶的当前行动方 + 先手侧（897 同款小工具扩展：返回 0/1 行动方）。
int j41_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1549 tick：MOVE_RIGHT 写攻击技先制 +{0} ──
EffectResult j41_1549_preempt_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int prio = a.int_args[2];
    if (self < 0 || self > 1 || prio <= 0) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(c, self)) {
        return EffectResult::kOk;   // "攻击技能"限定（681 同款）
    }
    c->ws.preemptive_level[self] += prio;
    return EffectResult::kOk;
}

// ── 1549 共用门探针：后出手 → 武装"下回合攻击技先制+{0}" ──
EffectResult j41_1549_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int prio = a.int_args[2];
    if (self < 0 || self > 1 || prio <= 0) {
        return EffectResult::kOk;
    }
    const int acting = j41_bucket_acting_side(c);
    if (acting != self) {
        return EffectResult::kOk;
    }
    // "后出手"：行动方是自身且自身非先手方。
    const int first = c->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    if (acting == first) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(c, State::BATTLE_FIRST_MOVE_RIGHT, self, 1549,
                                /*rounds=*/1, {self, 1 - self, prio},
                                &j41_1549_preempt_tick);
    return EffectResult::kOk;
}

// ── 1549 —— 后出手时下回合攻击技能先制+{0} ──
EffectResult effect_1549_second_preempt_next(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1549, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1549, args,
                     "布局=[owner,target,prio]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1549, args, "owner 越界");
    EFFECT_ARG_GUARD(args.int_args[2] > 0, 1549, args, "需要 prio>0");
    const std::vector<int> payload(args.int_args, args.int_args + 3);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1549, 0, self, -1, EffectArgs(payload), &j41_1549_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1549;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 662 —— {0}回合内免疫致命一击伤害 ──
EffectResult effect_662_immune_lethal_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 662, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 662, args,
                     "布局=[owner,target,rounds]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 662, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 662, args, "需要 rounds>0");
    ctx->grant_immunity(self, ImmunityType::INSTANT_KILL, coverage_all(),
                        /*anomaly_mask=*/0, rounds, /*source_id=*/662,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/662);
    return EffectResult::kOk;
}

// ── 648 共用 tick/watcher 体：对手下次攻击技未命中 → 申报制异常（一次性）──
EffectResult effect_648_miss_punish(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 648, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 648, args,
                     "布局=[owner,target,anomaly]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 648, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 648, args, "异常码越界");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 常驻 watcher（2300 同款 storage 句柄重绑——"下次"每次用技重新武装）。
    const int handle_key = 990119001 + self;
    const int old = ctx->plugin_storage.count(handle_key)
                        ? any_int_or(ctx->plugin_storage[handle_key])
                        : 0;
    if (old > 0) {
        ctx->event_center_.remove_watcher(old);
    }
    auto fired = std::make_shared<bool>(false);
    const int watcher_id = ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, self, ctx->roundCount, /*rounds=*/0,
        /*once=*/false,
        [self, opp, anomaly, fired](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || *fired) {
                return;
            }
            if (ev.actor != opp) {
                return;   // 只认对手的未命中
            }
            // "攻击技能"限定：miss 的这趟选择的仍是攻击技（roundChoice 保留可读）。
            if (!executed_skill_this_round(c, opp) || !executed_skill_is_attack(c, opp)) {
                return;
            }
            *fired = true;
            (void)g_core->apply_anomaly_checked(c, opp, anomaly,
                                                random_anomaly_duration(), self, 100,
                                                ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    ctx->plugin_storage[handle_key] = watcher_id;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1549, effect_1549_second_preempt_next)
SKILL_EFFECT(662, effect_662_immune_lethal_window)
SKILL_EFFECT(648, effect_648_miss_punish)
