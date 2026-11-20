// J 组 J266 · 能力下降随机异常族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1492 [1] 对手处于能力下降状态时随机附加{0}种异常状态
//
// 实现口径（先例：j_cond_pink_amp.cpp j35_has_any_stat_drop 六槽负等级判据；
// batch_a 1367 / j_rand_anom_same_elem 1291 的 attach_random_anomalies 原语）：
//   - 门 = 对手**任一能力槽 < 0**（六槽含命中；能力等级换宠清空，符合"处于"语义）。
//   - 随机附加 = attach_random_anomalies(对手, anomaly_pool_all(), {0}, 100%,
//     actor=自身, ChanceSource::Skill)：池解析/概率门/改写票/转化全收口在原语。
//     本模板无概率子句 → probability_pct=100；种数 {0} 在位即消费（<=0 时原语 requested=0）。
//   - 审计槽 991492200 + owner = 本次 landed 种数（0=门闭/未落地）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1492 [种数]：28260 敛财无道 [2]、28918 探宝时间 [1]、33625 狂·湮灭玄坤 [1]、
//     35208 K·疾尘羽落 [1]、35377 九转轮回天 [1]、35604 露露召唤兽 [1]、
//     37408 七月的海螺 [1]、38058 金坪镇华殿 [1]（8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1492 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1492) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5970_drop_rand_anomaly.cpp（号段 5970-5979）。

#include "common.h"

namespace {

constexpr int kAuditBase = 991492200;   // +owner：本次 landed 种数

bool has_any_stat_drop(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1492 —— 对手处于能力下降时随机附加{0}种异常 ──
// int_args = [owner, target, 种数]（args_num=1）。
EffectResult effect_1492_drop_gate_random_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1492, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1492, args,
                     "1492 布局=[owner,target,种数]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1492, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(count >= 0, 1492, args, "需要 种数 >= 0");
    if (!has_any_stat_drop(ctx, target)) {
        ctx->plugin_storage[kAuditBase + owner] = 0;
        return EffectResult::kOk;   // 门：对手不处于能力下降
    }
    if (count == 0 || !g_core || !g_core->attach_random_anomalies) {
        ctx->plugin_storage[kAuditBase + owner] = 0;
        return EffectResult::kOk;
    }
    const RandomAnomalyResult r = g_core->attach_random_anomalies(
        ctx, target, anomaly_pool_all(), count, /*probability_pct=*/100, owner,
        ChanceSource::Skill);
    ctx->plugin_storage[kAuditBase + owner] = r.landed;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1492, effect_1492_drop_gate_random_anomaly)
