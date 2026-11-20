// J32 · 受击窗响应族三 —— 84×9 / 427×20（2026-10-02，worktree ../agents/j32）
//
// 家族语义（effect_info 官方模板），两条都是"{0}回合内受到直接攻击 → 惩罚对手"：
//
// 84（arg_count=2）"{0}回合内受到物理攻击时有{1}%概率将对手麻痹"：
//   - 受击 watcher（817 同款：EVENT_TAKE_DAMAGE、TEAM scope、InRounds {0} 回合、
//     "物理攻击"= damage_kind==NORMAL、actor 为合法对手——粉伤 tick/真伤/属性直伤
//     不算，E12 口径）；
//   - 麻痹施加 = **申报制单掷**（apply_anomaly_checked chance={1}——464"遇天敌时{0}%
//     令对手烧伤"同形：{1}% 就是异常附加的申报概率，不做本地二次 roll；概率闸门
//     管辖）；对象 = 攻击者（ev.actor），异常码 Paralysis=0。
//   - 实例：雷光铠甲 20475 [5,50]、麻痹果汁 21136 [5,50]（全库 9 载体，全属性技，
//     {0}∈{3,5}）。
// 427（arg_count=2）"{0}回合内每次直接攻击都会使对手防御和特防{1}"：
//   - 同款 watcher；"直接攻击"= NORMAL 红伤（每次 qualifying 独立结算 → 可叠，钳 -6）；
//   - 弱化 = stat_drop(攻击者, 槽2/槽3, |{1}|, actor=自身)（弱化原语吃免弱——对手施加
//     的弱化走正规通道，actor 如实声明；全库 {1} 实载 -1，负号守卫）。
//   - 实例：启明祝愿 26502 [3,-1]、破甲之风 21880 [3,-1]（全库 20 载体，{1} 全为 -1）。
//
// 动工前置三查（2026-10-02）：①内核 grep 84/427 无行为硬编码 ②effect_meta 无分类
//   牵连 ③moves_lib 无既有注册——真缺口。载体数核对（EXISTS 口径）：9/20；全量切片
//   核验 29 载体布局零异常（84 [rounds,pct] / 427 [rounds,负增量]）。
// 先例：817（受击 watcher 注册法/判据）、464（申报制单掷）、J9 1322（受击→申报制
//   异常对攻击者）、1009（stat_drop 六槽循环）、场景 930（统计带断言先例）。
// 场景回归：test/scenario/scenario_1170_onhit_window3.cpp（1170 号段，一文件两宏）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 84 —— {0}回合内受到物理攻击时有{1}%概率将对手麻痹 ──
EffectResult effect_84_onhit_paralysis_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 84, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 84, args,
                     "布局=[owner,target,rounds,pct]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 84, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && pct > 0, 84, args, "需要 rounds>0 && pct>0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.target != self) {
                return;
            }
            // "物理攻击"= 红伤（粉伤 tick/真伤/属性直伤不触发，E12 口径）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == self) {
                return;   // 自伤不构成"对手攻击"
            }
            (void)g_core->apply_anomaly_checked(c, attacker,
                                                static_cast<int>(AbnormalStatusId::Paralysis),
                                                random_anomaly_duration(), self, pct,
                                                ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 427 —— {0}回合内每次直接攻击都会使对手防御和特防{1} ──
EffectResult effect_427_onhit_double_drop_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 427, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 427, args,
                     "布局=[owner,target,rounds,delta]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 427, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 427, args, "需要 rounds>0");
    EFFECT_ARG_GUARD(delta < 0, 427, args, "需要 delta<0（弱化字面；全库实载 -1）");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int amount = -delta;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, amount](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->stat_drop) {
                return;
            }
            if (ev.target != self
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)
                || ev.amount <= 0) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == self) {
                return;
            }
            // 防御（槽2）与特防（槽3）各降 amount（可叠，原语自钳 -6）。
            (void)g_core->stat_drop(c, attacker, 2, amount, self);
            (void)g_core->stat_drop(c, attacker, 3, amount, self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(84, effect_84_onhit_paralysis_window)
SKILL_EFFECT(427, effect_427_onhit_double_drop_window)
