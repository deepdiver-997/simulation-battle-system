#include <utils/sbs_rand.h>
// J 组 J223 · 随机吸取区间+强化门族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1694 [4] 随机吸取对手{0}-{1}点体力，若自身处于能力提升状态则效果转变为{2}-{3}点
//
// 实现口径：
//   - 区间抽取 = [lo, hi] 闭区间均匀随机（span = hi-lo+1；span<=1 不耗 rand）。
//   - 强化门 = 自身六个能力等级槽（含命中）任一 > 0（"处于能力提升状态"；J63/J71/J182 同判据）。
//     门开 → 用 {2}-{3} 档，门闭 → 用 {0}-{1} 档。**行参在位即消费**：门只选档，
//     不关闸（红线 7 语义：这里不是 pct 0 关闸族，档位参数照常参与）。
//   - 吸取 = 2274 点数吸血口径（J4 1022/1354 同款）：fixed_damage 走固伤通道（吃护罩/
//     免粉），按**实际扣减**回血（heal_amount，吃封回血与恢复修正）。引擎的
//     drain_hp_amount 未暴露进 CoreApi（core_api.h 记档），故走双原语。
//   - 审计槽 plugin_storage[990169400 + owner*2]     = 本次抽取点数；
//     plugin_storage[990169400 + owner*2 + 1]       = 强化门（1=门开/{2}-{3} 档，0=门闭）。
//     +owner*2 避免跨侧相邻键撞键（J207/J209 勘误口径）。
//
// 载体切片（side_effect_arg 按链序 + 各 effect_info.args_num 位切，2026-10-22 脚本核算，
// 9/9 自洽；1694 固定 4 位 = [lo,hi,lo_boost,hi_boost]）：
//   28981 盘根错节 [191,576,1769,1694]|[4,4,300,2,1,2,6,200,300,300,350] → 1694=[200,300,300,350]
//   29029 妙手童话 [946,1694,524]|[1,200,300,300,400,3,3]            → 1694=[200,300,300,400]
//   35072 沐曦皆烬燃 [1690,1221,1694]|[1,1,200,300,300,400]          → 1694=[200,300,300,400]
//   35642 遍鳞伏地击 [1694,1092]|[150,300,200,350,6,3]               → 1694=[150,300,200,350]
//   36422 忘尘录 [1129,1694]|[25,2,150,250,200,300]                  → 1694=[150,250,200,300]
//   36733 花月鸟之梦 [1099,1694,1821,1960]|[1,200,300,300,400,2,2]   → 1694=[200,300,300,400]
//   36813 凌云行 [794,1694]|[2,200,300,300,400]                      → 1694=[200,300,300,400]
//   36973 生机重锤 [1243,1048,1694]|[100,200,350,300,450]            → 1694=[200,350,300,450]
//   38039 妄语 [1081,948,1694]|[23,200,300,300,400]                  → 1694=[200,300,300,400]
//   场景取 28981（属性技，链兄弟 191 免异常/576 低伤免疫/1769 未注册——均不碰能力等级）
//   作门闭档；29029（属性技，946 全属性+{0} 先手强化，天然开门）作门开档。
//
// 动工前置三查（2026-10-22）：①内核 grep 1694 无行为硬编码、parser 模板白名单不含本条
//   ②effect_info 无同文孪生 ③moves_lib 并集（含 kbatch worktree 未合并分支）无 1694 注册。
//
// 场景回归：test/scenario/scenario_5530_random_drain_range.cpp（号段 5530-5539）。

#include "common.h"

namespace {

constexpr int k1694AuditBase = 990169400;   // + owner*2：抽取点数；+1：强化门

// "自身处于能力提升状态" = 六槽（含命中）任一正等级（J63/J71/J182 同判据）。
inline bool j223_has_stat_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 点数吸血（2274 口径：fixed_damage 走固伤通道吃护盾/免粉，按**实际扣减**回血、
// 回血吃封回血与恢复修正）。
void j223_drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!ctx || !g_core || !g_core->fixed_damage || !g_core->heal_amount || points <= 0) {
        return;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
}

// ── 1694 —— 随机吸取对手{0}-{1}点体力；自身有强化则改用{2}-{3}档 ──
// int_args = [owner, target, lo, hi, lo_boost, hi_boost]（args_num=4）。
EffectResult effect_1694_random_drain_range(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1694, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1694, args,
                     "1694 布局=[owner,target,lo,hi,lo_boost,hi_boost]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    const int lo_boost = args.int_args[4];
    const int hi_boost = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1694, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(lo > 0 && hi >= lo && lo_boost > 0 && hi_boost >= lo_boost, 1694, args,
                     "需要 0 < lo <= hi 且 0 < lo_boost <= hi_boost");

    const bool boosted = j223_has_stat_boost(ctx, owner);
    const int range_lo = boosted ? lo_boost : lo;
    const int range_hi = boosted ? hi_boost : hi;
    const int amount = choose_skill_range(*ctx, 1694, 0, owner, target, range_lo, range_hi,
                                          ChanceSingleton::SkipDraw);

    ctx->plugin_storage[k1694AuditBase + owner * 2] = amount;
    ctx->plugin_storage[k1694AuditBase + owner * 2 + 1] = boosted ? 1 : 0;
    j223_drain_points(ctx, owner, target, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1694, effect_1694_random_drain_range)
