// J 组 J264 · 能力下降吸取族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1225 [1] 对手处于能力下降时吸取对手最大体力的1/{0}
//
// 实现口径（先例：j35_has_any_stat_drop 六槽负等级判据（j_cond_pink_amp.cpp）；
// j_drain_cond.cpp / j_hp_ratio_pink_drain.cpp 1137 drain_hp 通道）：
//   - 门 = 对手**任一能力槽 < 0**（六槽含命中；能力等级换宠清空，符合"处于"语义）。
//   - 数值 = drain_hp(actor=自身, target=对手, {0})：目标掉 max_hp/{0} 固定伤害、
//     自身回等量（实际落地量，受封回血影响；坑集§9：粉伤落地与回血解耦是原语口径）。
//   - 分母在位即消费：{0} <= 0 由 EFFECT_ARG_GUARD 响亮报错，不做静默关闸。
//   - 审计槽 991225200 + owner = 本次吸取名义量（0=门闭/目标已倒）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1225 [分母]：31051 圣堂皇家剑 [3]、32217 千金归来诀 [3]、32710 凤焰炽火阵 [3]、
//     32860 灵魂永冻曲 [3]、36398 白金旋风拳 [3]、36745 堕·决赴黑渊 [3]、
//     36928 缚影溃光诀 [3]、37841 极冰影闪 [3]（8/8 全分母 3）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1225 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1225) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5950_drain_on_stat_drop.cpp（号段 5950-5959）。

#include "common.h"

namespace {

constexpr int kAuditBase = 991225200;   // +owner：本次吸取名义量（0=门闭）

// 对手处于能力下降状态 = 六槽（含命中）任一负等级（J14/J17/J35 同判据）。
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

// ── 1225 —— 对手处于能力下降时吸取对手最大体力的1/{0} ──
// int_args = [owner, target, 分母]（args_num=1）。
EffectResult effect_1225_drain_if_stat_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1225, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1225, args,
                     "1225 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1225, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1225, args, "需要 分母 > 0");
    if (!has_any_stat_drop(ctx, target)) {
        ctx->plugin_storage[kAuditBase + owner] = 0;
        return EffectResult::kOk;   // 门：对手不处于能力下降
    }
    const int amount = max_hp_of(ctx, target) / denom;
    if (amount <= 0 || ctx->getPet(target).hp <= 0) {
        ctx->plugin_storage[kAuditBase + owner] = 0;
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kAuditBase + owner] = amount;
    if (g_core && g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, owner, target, denom);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1225, effect_1225_drain_if_stat_drop)
