// J37 · 未击败条件族（本单产码部分）—— 957×20（2026-10-02，worktree ../agents/j37）
//
// ⚠️ 本单 3 宏中 964/920 与 **feat/j12（J12 未击败门族二）重复立项**——j12 已实现
//   964（正槽翻倍）/ 920（下回合增伤 NextRounds tick）/ 719 / 956 四宏
//   （j_defeat_gate2.cpp，897 AFTER_ACTION 桥共门），本分支未并、不重复注册：
//   → 964/920 记档跳过，只产 **957**。
//
// 957（arg_count=3）"使对手全属性-{0}，若对手处于{1}状态则自身额外全属性+{2}"：
//   - stat_drop 六槽循环（对手，量 {0}，actor=自身——弱化原语吃免弱，1009 同款）；
//   - 条件 = has_active_anomaly(对手, {1})（{1}=具体异常码，全载 {2,5,6,8,9}）→
//     all_stats_up(自身, +{2})（common.h 六槽 stat_change 真原语，自身增益不吃免弱）。
//   - 实例：鬼鬼祟祟 28254 [1,2,1]（全库 20 载体，{0}/{2} 全为 1）。
//
// 动工前置三查（2026-10-02）：①内核 grep 957 无行为硬编码 ②effect_meta 无分类牵连
//   ③moves_lib（本分支）无既有注册——真缺口。载体切片核验 20 载体布局零异常
//   （[降槽量,异常码,额外量]，{0}/{2} 全 1）。
// 先例：1009/938（stat_drop 六槽循环）、all_stats_up（common.h）、1837（自增益）。
// 场景回归：test/scenario/scenario_1220_957.cpp（1220 号段，单宏单块）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 957 —— 使对手全属性-{0}，若对手处于{1}状态则自身额外全属性+{2} ──
EffectResult effect_957_all_down_cond_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 957, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 957, args,
                     "布局=[owner,target,drop,anomaly,extra]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int drop = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int extra = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 957, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(drop > 0 && extra > 0, 957, args, "需要 drop>0 && extra>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 957, args, "异常码越界");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, drop, self);
    }
    if (g_core->has_active_anomaly(ctx, opp, anomaly)) {
        all_stats_up(ctx, self, extra);   // "额外全属性+"（六槽 stat_change，不吃免弱）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(957, effect_957_all_down_cond_up)
