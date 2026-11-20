// J 组 J231 · 低体概率异常族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   159 [3] 自身体力小于最大值的1/{0}时，{1}%概率令对方{2}
//
// 实现口径（直系先例 j_prob_anomaly_low_hp.cpp 1210：HP 门 + 申报制异常）：
//   - SKILL_EFFECT 时点同步读**当前**体力：self.hp < max_hp/{0}（**严格小于**；
//     恰好等于阈值不触发——"小于"字面）。阈值读 numericalBase[HP]（max_hp_of）。
//   - 门开 → apply_anomaly_checked(opp, {2}, random_anomaly_duration(), self, {1},
//     ChanceSource::Skill)：概率申报给原语（亮节闸门/抗性链全在内部），异常时长沿用
//     既有施加族口径（1210/1030 同款）。
//   - pct 在位即消费（红线 7）：{1}=0 是显式关闸档（仍申报、掷点必败），不设 >0 门。
//   - 审计槽 plugin_storage[990159000 + self*2]     = 本次申报概率 {1}；
//     plugin_storage[990159000 + self*2 + 1]       = ApplyAnomalyResult 整数；门闭记 -1。
//     +self*2 避免跨侧相邻键撞键（J207/J209 勘误口径）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   159 [分母,概率,异常码]：12788 绝地影袭 [4,50,6害怕]、14976 狂蛇地灵击 [2,50,6]、
//     15437 次元圣锤 [3,50,6]、29435 扭曲片像 [2,100,0麻痹]、36869 幻梦之引 [1,100,8睡眠]、
//     37655 迟波抚镜 [2,100,6]、37701 寒境吞风破 [2,70,15冰封]、38289 自毁冲击 [2,100,7疲惫]
//     （1/2/3/4 分母档、50/70/100 概率档、6/0/8/15/7 异常档）
//
// 动工前置三查（2026-10-22）：①内核 grep 159 无硬编码 ②effect_info 无同文模板
//   ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(159)——真缺口。
//
// 场景回归：test/scenario/scenario_5620_low_hp_chance_anomaly.cpp（号段 5620-5629）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k159PctBase = 990159000;     // +self*2：本次申报概率
constexpr int k159ResultBase = 990159001;  // +self*2：ApplyAnomalyResult 整数（门闭 -1）

// ── 159 —— 自身体力小于最大值的1/{0}时，{1}%概率令对方{2} ──
// int_args = [owner, target, 分母, 概率, 异常码]（args_num=3）。
EffectResult effect_159_low_hp_chance_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 159, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 159, args,
                     "159 布局=[owner,target,分母,概率,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int pct = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     159, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 159, args, "需要 分母 > 0");
    // 红线7：概率在位即消费（0 = 显式关闸档，仍申报、掷点必败——不得当非法参数）。
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 159, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 159, args, "需要合法异常 id");

    ctx->plugin_storage[k159PctBase + self * 2] = pct;

    // 门：当前体力严格小于 max_hp/denom。
    const int max_hp = max_hp_of(ctx, self);
    if (ctx->getPet(self).hp >= max_hp / denom) {
        ctx->plugin_storage[k159ResultBase + self * 2] = -1;   // 门闭不申报
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, pct, ChanceSource::Skill);
    ctx->plugin_storage[k159ResultBase + self * 2] = static_cast<int>(r);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(159, effect_159_low_hp_chance_anomaly)
