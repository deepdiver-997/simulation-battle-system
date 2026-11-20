// J 组 J312 · 条件先制族三（异常门 / 体力比较门）—— 1956×7 + 1292×7
//（2026-10-23，worktree feat/jbatch；号段 6450-6459）
//
// 家族语义（effect_info 官方模板即权威）：
//   1956 [0] 若对手不处于异常状态则先制+1
//   1292 [1] 自身体力低于对手时先制+2
//
// 实现口径（J115 条件先制族二 j_cond_prio2.cpp 同款 + 954 低体先制 crawford_skills.cpp）：
//   - 两条都是**选择期条件先制**——先手权在选技阶段就定；注册进用技期分支（case 路由）
//     赶不上首回合先制判定（J66 勘误：case 版先手断言必挂）。故须在
//     src/entities/skills.cpp 的 selection_effects_ 路由表加 1956/1292 两行
//     （on_selected 统一注册到 BATTLE_FIRST_MOVE_RIGHT 桶 → 先手权比较前生效）。
//   - 1956 条件 = 对手**任一**异常（has_active_anomaly 扫 0..43 全集，J86
//     j_status_gate_damage.cpp 判据同款）——"不处于异常"= 一个都不处于。
//     零参 → 先制量写死 1（模板文案即 +1）。
//   - 1292 条件 = 自身体力 < 对手体力（954「若当前体力低于对手则先制+1」同款整数
//     直比，严格小于）；先制量取**行参**（args[2]，载体恒 2）——行参在位即消费，
//     不写死文案数字（红线 7 口径：显式 0 也照消费，不加 >0 守卫）。
//   - 审计槽：991956100+self = 1956 触发标记（1 = 授予 +1）；
//     991292100+self = 1292 授予的先制量（0 = 未授予）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-23 脚本核算）：
//   1956 [] 7/7 自洽（零参，"没有参数"）：
//     36530 寒囚凝天刃 [1956,1111,833]、36652 嫣然灼魂叹 [1956,1734,1163,698]、
//     36861 星火搜寻 [1956,1659]、37310 劫数·噩咒堕天 [1956,1114,1334,1709,1049]、
//     37359 霜影猎魔刃 [1956,1441,1111,1709]、37472 流焰行军 [494,1956,879,1048]、
//     38433 创·黄金纪元 [1956,2034,1680,1163]。
//   1292 [先制量] 7/7 自洽（恒 2）：
//     25905 用武通神 [1292,48,1140,738]、26955 地幔之兆 [1292,4,4,1369]、
//     32401 年少万兜鍪 [1292,1141]、32647 邪灵花蕊 [1292]（**纯链**）、
//     32694 元素毁灭阵 [1292,496,732]、32851 仙云腾空击 [1292,479]、
//     36257 沧海无言 [1292,1777,1702]。
//
// 动工前置三查（2026-10-23 本 worktree 复核）：①内核 grep 1956/1292 无行为硬编码
//   ②effect_info 模板唯一（各 1 行）、custom_effect_programs/overrides 零行
//   ③全库 moves_lib/soul_lib 并集（含 kbatch / hub / agents 各 worktree）无
//   SKILL_EFFECT(1956/1292) 注册——真缺口。
//
// 先例：958/1174/1081（条件先制选择期路由，j_cond_prio2.cpp）、954（低体先制，
//   crawford_skills.cpp）、J86（任一异常判据，j_status_gate_damage.cpp）。
// 场景回归：test/scenario/scenario_6450_cond_prio3.cpp（号段 6450-6459）。

#include "common.h"

namespace {

constexpr int kAudit1956 = 991956100;   // +self = 触发标记
constexpr int kAudit1292 = 991292100;   // +self = 授予的先制量（0 = 未授予）

// 对手**任一**异常（BattleContext::has_active_abnormal_status 非 inline、插件不可
// 直调 → 走 CoreApi 槽 has_active_anomaly 扫全集 0..43，J86 同款判据）。
bool j312_has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= 43; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 1956 —— 若对手不处于异常状态则先制+1（零参；选择期路由体）──
// int_args = [owner, target]（on_selected 绑定；零参效果无第三个槽）。
EffectResult effect_1956_no_anomaly_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1956, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1956, args,
                     "1956 布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1956, args, "需要 owner/target ∈ {0,1} 且互异");
    if (j312_has_any_anomaly(ctx, opp)) {
        return EffectResult::kOk;   // 门闭：对手处于异常状态 → 不加先制
    }
    ctx->ws.preemptive_level[self] += 1;
    ctx->plugin_storage[kAudit1956 + self] = 1;
    return EffectResult::kOk;
}

// ── 1292 —— 自身体力低于对手时先制+{0}（行参在位即消费）──
// int_args = [owner, target, 先制量]（args_num=1）。
EffectResult effect_1292_low_hp_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1292, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1292, args,
                     "1292 布局=[owner,target,先制量]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int bonus = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1292, args, "需要 owner/target ∈ {0,1} 且互异");
    // 严格低于：自身体力 < 对手体力（954 同款整数直比）。
    if (ctx->getPet(self).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // 门闭：体力不低于对手 → 不加先制
    }
    ctx->ws.preemptive_level[self] += bonus;
    ctx->plugin_storage[kAudit1292 + self] = bonus;
    return EffectResult::kOk;
}

SKILL_EFFECT(1956, effect_1956_no_anomaly_priority)
SKILL_EFFECT(1292, effect_1292_low_hp_priority)
