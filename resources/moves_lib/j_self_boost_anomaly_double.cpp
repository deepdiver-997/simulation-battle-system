// J 组 J275 · 自身六槽增益·异况翻倍族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   562 [7] "自身{0}，对手{6}时强化效果翻倍"
//   args[0..5] = 六槽增量（官方码序 0攻 1防 2特攻 3特防 4速 5命中），
//   args[6] = 触发翻倍的对手异常码。
//
// 实现口径：
//   - 六槽 → stat_change(self, kStatMap[i], delta×mult)（自身增益不吃免弱；
//     本体/视图同步）。kStatMap = {0,2,1,3,4,5}（官方码序→引擎槽，J48/J81 同表）。
//   - 门 = g_core->has_active_anomaly(对手, args[6])（精确异常码；575/2009 同查询面）
//     → mult=2，否则 1。
//   - 审计槽 991562000+owner = 本次 mult（1/2），场景可无随机断言。
//   - 六槽增量与异常码在位即消费（0 增量合法空操作；异常码 0=麻痹也照常查询）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   562 [六槽×6, anomaly]：22961 火焰战魂 [0,1,1,1,1,0,2]（纯链，场景载体）、
//     23684 烈焰火炮 [1,1,1,1,1,1,2]、23711 叶绿之力 [0,1,1,1,0,0,1]（纯链）、
//     23762 龙怒焚天 [1,1,0,1,1,0,2]、23993 振翼强风 [0,2,0,2,0,0,8]（纯链）、
//     24017 神炎狂威 [1,1,1,1,1,1,2]、24246 冰芒护体 [1,1,1,1,1,1,5]、
//     24274 炽·不灭之火 [1,1,0,1,1,0,2]、29101 毒素逸散 [0,1,1,1,1,0,1]（9/9 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 562 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(562) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6080_self_boost_anomaly_double.cpp（号段 6080-6089）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kAuditBase = 991562000;   // +owner：本次倍率（1/2）

// 官方码序（0攻 1防 2特攻 3特防 4速 5命中）→ 引擎能力槽（J48/J81 同表）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

// ── 562 —— 自身{0}，对手{6}时强化效果翻倍 ──
// int_args = [owner, target, 六槽×6, 异常码]（args_num=7）。
EffectResult effect_562_self_boost_anomaly_double(BattleContext* ctx,
                                                  const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 562, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 9, 562, args,
                     "562 布局=[owner,target,六槽×6,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[8];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     562, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 562, args,
                     "异常码越界（≤43）");

    int mult = 1;
    if (g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, opp, anomaly)) {
        mult = 2;
    }
    ctx->plugin_storage[kAuditBase + self] = mult;
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        if (delta != 0) {
            (void)g_core->stat_change(ctx, self, kStatMap[i], delta * mult);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(562, effect_562_self_boost_anomaly_double)
