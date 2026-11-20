// J33 · 条件弱化/状态强化族 —— 414×5 / 175×9 / 690×11（2026-10-02，worktree ../agents/j33）
//
// 家族语义（effect_info 官方模板），三条都以"对手异常状态"为条件轴：
//
// 414（arg_count=3）"对手{0}等级{1}，若对手处于{2}状态，则弱化效果翻倍"：
//   - stat_drop(对手, 槽{0}, |{1}| × (has_active_anomaly(对手,{2}) ? 2 : 1), actor=自身)
//     （"弱化效果翻倍"= 量 ×2，403 取法；弱化原语吃免弱；{0}=能力槽 0攻1特攻2防
//     3特防4速5命中——载体验证：万丈冰渊 [4,-1,5]=速度-1、寒风侵蚀 [3,-2,5]=特防-2）；
//     {2}=具体异常码（全载 {5,7,16,1}），不是"任意异常"。
//   - 实例：寒风侵蚀 34378 [3,-2,5]、万丈冰渊 21775 [4,-1,5]（全库 5 载体）。
// 175（arg_count=3）"若对手处于异常状态，则{1}%自身{0}等级{2}"：
//   - has_any_active_abnormal_status(对手) → roll_percent({1}) → stat_change(自身,
//     槽{0}, {2})（自增益真原语不吃免弱——common.h effect 4 口径；{2} 正值）。
//   - 实例：星辰之舞 12951 [2,100,1]、寒霜利爪 18299 [0,100,2]（全库 9 载体，
//     {1} 全为 100——确定性断言）。
// 690（arg_count=1）"下{0}回合，能力提升状态消失则对手使用属性技能失效"：
//   - E13 EVENT_STAT_BOOSTS_LOST watcher（573 同款注册法；"消失"=消除 kind=0 与
//     吸取 kind=1 都算——字面无"消除"限定，取宽记档），窗 = **NextRounds {0}**
//     （"下{0}回合"——827/681 起点口径）；触发 → 478 逐参封属（1483 直系），
//     封属时长 = 窗剩余回合（expire - 当前；官方未给时长，取"随窗截止"保守值，记档）。
//   - 实例：神圣领域 23771 [3]、翩鸿幻彩 24399 [3]（全库 11 载体，{0} 全为 3）。
//
// 动工前置三查（2026-10-02）：①内核 grep 414/175/690 无行为硬编码 ②effect_meta
// 无分类牵连 ③moves_lib 无既有注册——真缺口。载体数核对（EXISTS 口径）：5/9/11；
// 全量切片核验 25 载体布局零异常（414 [槽,负增量,异常码] / 175 [槽,概率,正增量] /
// 690 [rounds]）。⚠️ 崩坏怒意链上 585 是"任意效果模板"（args_num=6 吞子句，650/732
// 同族缓办）——690 场景载体避开它，用翩鸿幻彩（48/507/776 均不碰触发/封属面）。
// 先例：1248/403（条件翻倍取法）、J9 1594（roll_percent→stat_change）、573（E13
//   watcher 窗）、1483/J18（478 封属）、827/681（NextRounds 起点）。
// 场景回归：test/scenario/scenario_1180_cond_drop.cpp（1180 号段，一文件三宏）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 414 —— 对手{0}等级{1}，若对手处于{2}状态，则弱化效果翻倍 ──
EffectResult effect_414_cond_double_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 414, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 414, args,
                     "布局=[owner,target,slot,delta,anomaly]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int slot = args.int_args[2];
    const int delta = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 414, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 414, args,
                     "槽位越界");
    EFFECT_ARG_GUARD(delta < 0, 414, args, "需要 delta<0（弱化字面）");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 414, args, "异常码越界");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int amount = -delta * (g_core->has_active_anomaly(ctx, opp, anomaly) ? 2 : 1);
    (void)g_core->stat_drop(ctx, opp, slot, amount, self);
    return EffectResult::kOk;
}

// ── 175 —— 若对手处于异常状态，则{1}%自身{0}等级{2} ──
EffectResult effect_175_anomaly_self_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 175, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 175, args,
                     "布局=[owner,target,slot,chance,amount]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int slot = args.int_args[2];
    const int chance = args.int_args[3];
    const int amount = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 175, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 175, args,
                     "槽位越界");
    EFFECT_ARG_GUARD(chance > 0 && amount > 0, 175, args, "需要 chance>0 && amount>0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(opp)) {
        return EffectResult::kOk;   // 前提：对手处于（任意）异常状态
    }
    if (roll_percent(chance)) {
        (void)g_core->stat_change(ctx, self, slot, amount);
    }
    return EffectResult::kOk;
}

// ── 690 —— 下{0}回合，能力提升状态消失则对手使用属性技能失效 ──
EffectResult effect_690_boost_lost_seal_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 690, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 690, args,
                     "布局=[owner,target,rounds]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 690, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 690, args, "需要 rounds>0");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // 窗 = "下{0}回合"（NextRounds 起点，827/681 口径）。
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::NextRounds);
    const int expire = start + rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, start, rounds, /*once=*/false,
        [self, start, expire](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->seal_skill) {
                return;
            }
            // "能力提升状态消失"：自身的提升被消除(0)或吸取(1)（字面取宽，记档）。
            if (ev.target != self || ev.stat_loss_kind < 0) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗自检（watcher 窗与桶窗同参数，双保险）
            }
            // 封属时长 = 窗剩余回合（官方未给时长，随窗截止——文件头记档）。
            const int seal_rounds = expire - c->roundCount;
            (void)g_core->seal_skill(c, /*source=*/self, /*target=*/1 - self,
                                     /*effect_id=*/690, /*attribute=*/true, /*attack=*/false,
                                     /*count=*/0, /*duration_rounds=*/seal_rounds,
                                     /*penetrable=*/true, /*source_slot=*/-1,
                                     EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                     /*chance_pct=*/100, /*consumed_when_pierced=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(414, effect_414_cond_double_drop)
SKILL_EFFECT(175, effect_175_anomaly_self_boost)
SKILL_EFFECT(690, effect_690_boost_lost_seal_window)
