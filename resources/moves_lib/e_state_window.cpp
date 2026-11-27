// E13 · 状态窗口族 —— 469×63 / 573×60 / 477×38（2026-09-29，worktree ../agents/e13）
//
// 家族语义："N回合内若<条件>则<后果>"——窗口期内**实时监听条件事件**。
//
// 内核（随本文件落地，两个新事件）：
//   EVENT_SKILL_USED —— Skills::execute 起点 emit（"使用即触发"：miss/被封属/打盔
//     都算"使用"，与 EVENT_HIT 的"命中才发"正交）；used_attribute 标记属性技。
//   EVENT_STAT_BOOSTS_LOST —— clear_stat_boosts（消除）/ transfer_stat_boosts（吸取）
//     成功路径 emit；stat_loss_kind 区分 0 消除 / 1 被吸取。
//   477 的"若受到攻击"用**现有** EVENT_HIT（actor=攻击方、target=防御方，
//   命中才发——"受到攻击"字面即被打到），零内核改动。
//
// 469（args [rounds, chance, anomaly]）"{0}回合内若对手使用属性技能则{1}%令对手{2}"：
//   watcher EVENT_SKILL_USED，filter ev.target==self && ev.used_attribute →
//   roll → 对**使用方**（ev.actor）施加异常。
//   实例：慈辉永颂·瑞亚 神离之始 29314（3回合、100%、害怕——"对手敢开强化技就怕"）。
// 573（args [rounds, chance, anomaly, dur]）"{0}回合内若自身能力提升状态被消除或吸取
//   则{1}%使对手{2}{3}回合"：watcher EVENT_STAT_BOOSTS_LOST，filter ev.target==self
//   （消除/吸取两种 stat_loss_kind 都触发）→ roll → 对手异常 {3} 回合（指定时长，
//   非 random_anomaly_duration——模板带 {3} 回合参数）。
//   实例：圣能·虚无 圣域之劫 24275（3回合、50%、害怕2回合）。
// 477（args 7 [rounds, 攻Δ, 防Δ, 特攻Δ, 特防Δ, 速Δ, 命中Δ]）"{0}回合内若受到攻击，
//   则对手{1}"：watcher EVENT_HIT filter ev.target==self → 六槽按表 stat_drop
//   （走免弱 STAT_DROP 检查的正规通道；Δ=0 的槽跳过）。
//   实例：波尼啵妮 环流回声 28748（3回合、防-2特防-2）、创世圣戒 地灵之缚 22645。
//
// 窗口口径："{0}回合内"含当回合（1188 受击窗口同款，duration=rounds、TEAM scope——
// ON_STAGE 的 valid_id 活不过回合交界，1219/E2 已踩）。
//
// 前置三查（2026-09-29）：①内核原无三效果处理（BEFORE_SKILL_HIT 是每回合一次的
// 去重锚点，不满足"每次受攻击"/"使用即触发"语义——真缺口）②effect_meta 无穿透分类
// ③全库无既有实现。
//
// 场景回归：test/scenario/scenario_590_state_window.cpp。

#include "common.h"

namespace {

// ── 469 —— {0}回合内若对手使用属性技能则{1}%令对手{2} ──
EffectResult effect_469_attr_use_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (self < 0 || self > 1 || rounds <= 0 || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly) {
                return;
            }
            // 条件：**对手**使用的是属性技（使用即触发，命中与否无关）
            if (ev.target != self || !ev.used_attribute) {
                return;
            }
            const int user = ev.actor;
            if (user < 0 || user > 1) {
                return;
            }
            if (roll_percent(c->rng(), chance)) {
                (void)g_core->apply_anomaly(c, user, anomaly, random_anomaly_duration(), self);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 573 —— {0}回合内若自身能力提升状态被消除或吸取则{1}%使对手{2}{3}回合 ──
EffectResult effect_573_boost_lost_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    const int dur = args.int_args[5];
    if (self < 0 || self > 1 || rounds <= 0 || dur <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, anomaly, dur](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly) {
                return;
            }
            // 条件：**自身**的提升被消除或吸取（两种 kind 都算——字面"被消除或吸取"）
            if (ev.target != self || ev.stat_loss_kind < 0) {
                return;
            }
            if (roll_percent(c->rng(), chance)) {
                (void)g_core->apply_anomaly(c, 1 - self, anomaly, dur, self);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 477 —— {0}回合内若受到攻击，则对手{1}（六槽降级表）──
// args[3..8] 是**语义序**（攻,防,特攻,特防,速,命中）——环流回声"防御-2 特防-2"
// = [0,-2,0,-2,0,0]、地灵之缚"速度命中-1" = [0,0,0,0,-1,-1]、自然平衡"双攻速-1"
// = [-1,0,-1,0,-1,0] 三例交叉验证；等级槽序 = 0攻 1特攻 2防 3特防 4速 5命中
//（skill_type+2 口径）→ 语义序到槽序映射 {0,2,1,3,4,5}。
EffectResult effect_477_hit_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 9 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    static constexpr int kSemanticToSlot[6] = {0, 2, 1, 3, 4, 5};
    // 六槽降级表：args[3..8] = 攻/防/特攻/特防/速/命中 的 Δ（负值生效，0 跳过）
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount, rounds, /*once=*/false,
        [self, drops = std::vector<int>{args.int_args[3], args.int_args[4], args.int_args[5],
                                        args.int_args[6], args.int_args[7], args.int_args[8]}](
            BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->stat_drop) {
                return;
            }
            if (ev.target != self) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1) {
                return;
            }
            for (int i = 0; i < 6; ++i) {
                if (drops[i] < 0) {
                    (void)g_core->stat_drop(c, attacker, kSemanticToSlot[i], -drops[i], self);
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(469, effect_469_attr_use_window)
SKILL_EFFECT(573, effect_573_boost_lost_window)
SKILL_EFFECT(477, effect_477_hit_drop)
