// J 组 J242 · 随机多异常族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   549（args_num=1）"随机附加烧伤、冻伤、中毒、害怕、疲惫、麻痹其中{0}种异常状态"
//
// 实现口径（1367/1865 同通道——随机异常附加原语）：
//   - 池 = 官方文案六项，按**文案序**：烧伤 2 / 冻伤 5 / 中毒 1 / 害怕 6 /
//     疲惫 7 / 麻痹 0（AbnormalStatusId 官方码；不是"全池"）。
//   - attach_random_anomalies(对手, pool, count={0}, 概率门=100, actor=owner,
//     ChanceSource::Skill)：池解析（⊕ ANOMALY_POOL_MOD）→ 子句级概率门 →
//     **不重复**挑 {0} 个 → 逐个走 apply_anomaly 单通道（免疫/抗性/弹控/转化全继承）。
//   - 红线7：{0}=0 在位即消费（传 0 → requested=0 的自然 no-op），不设 >0 门；
//     负数是参数故障 → guard。
//   - 审计槽 plugin_storage[990549100+owner] = requested*1000 + landed
//     （场景断言"真的挑到了 {0} 种且全部落地"）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   549 [种数]：34810 凋零之触 [2]（物 150）、22871 天意 [2]（属性）、
//     28512 鬼影把戏 [1]（属性）、35404 怪味汤 [1]（特 140）、37125 余波 [1]（特 45）、
//     25453 石林之境 [549,191,462]|[2,4,3,250]、29453 肆想片像 [191,549,583]|[4,3,3]、
//     38405 六色七彩 [176,549]|[100,1]（8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 549 无硬编码 ②effect_info 模板唯一、
// custom_effect_overrides/programs 无行 ③registry.json moves 集不含 549，全库
// moves_lib/soul_lib 并集（含各 worktree）无 SKILL_EFFECT(549)——真缺口。
// 场景回归：test/scenario/scenario_5730_random_multi_anomaly.cpp（号段 5730-5739）。

#include "common.h"

#include <vector>

namespace {

constexpr int k549AuditBase = 990549100;   // +owner：requested*1000 + landed

// 官方文案序：烧伤、冻伤、中毒、害怕、疲惫、麻痹（不是全池）。
const std::vector<int>& pool_549() {
    static const std::vector<int> kPool = {2, 5, 1, 6, 7, 0};
    return kPool;
}

// ── 549 —— 随机附加<六种异常>其中{0}种 ──
// int_args = [owner, target, 种数]（args_num=1）。
EffectResult effect_549_random_multi_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 549, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 549, args,
                     "549 布局=[owner,target,种数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     549, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count >= 0, 549, args, "需要 种数 ≥ 0（0 在位即消费：自然 no-op）");
    if (!g_core || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, target, pool_549(), count, /*probability_pct=*/100, owner, ChanceSource::Skill);
    ctx->plugin_storage[k549AuditBase + owner] = r.requested * 1000 + r.landed;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(549, effect_549_random_multi_anomaly)
