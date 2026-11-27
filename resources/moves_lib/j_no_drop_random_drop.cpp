#include <utils/sbs_rand.h>
// J 组 J247 · 无降态随机降族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1417（args_num=2）"对手不处于能力下降状态则使对手随机{0}项能力值-{1}"
//
// 实现口径：
//   - 门 = 对手 6 个能力槽的**视图等级**没有任何负值（"不处于能力下降状态"；
//     读 `ws.view_levels`，与伤害公式同源）。
//   - 落点 = 从 0..5 六槽中随机抽 {0} 个**互异**槽，逐个 `stat_drop(对手, 槽, {1})`
//     （849/626 的 pool+erase 取法；走弱化原语 → 免弱/附加禁令自然接管，钳 -6）。
//   - `{0}`/`{1}` 在位即消费：<=0 = 不降（不回退默认值）。
//   - 抽取不预筛已满 -6 的槽（849 同口径：随机选中后由原语钳制）。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 8/8 自洽）：
//   32741 [970,449,1417]|[1,60,6,3,1] / 32860 [972,1225,1417]|[1,3,2,1] /
//   33504 [913,1417]|[100,3,1] / 35099 [1034,1244,1417]|[1,100,300,200,400,3,1] /
//   35213 [1244,1417]|[150,250,250,350,3,1] /
//   **36079 陷场龙啸 [1417]|[3,1]（纯链，场景载体）** /
//   37449 [1417,1660,1276]|[4,1,100,8,5,2] /
//   **37617 惊惶缭绕 [1417]|[3,2]（纯链，场景载体，验降幅）**。
//
// 动工前置三查（2026-10-22）：①内核 grep 1417 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree + 全部本地分支）
//   无注册——真缺口。
//
// 场景回归：test/scenario/scenario_5780_no_drop_random_drop.cpp（号段 5780-5789）。

#include "common.h"

namespace {

// ── 1417 —— 对手不处于能力下降状态则使对手随机{0}项能力值-{1} ──
EffectResult effect_1417_no_drop_random_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1417, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1417, args,
                     "1417 布局=[owner,target,count,amount]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1417,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：0/负档 = 不降（不回退默认值）。
    if (count <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    // "对手不处于能力下降状态" = 视图六槽无任何负值。
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ws.view_levels[opp][i] < 0) {
            return EffectResult::kOk;
        }
    }
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    std::vector<int> pool;
    pool.reserve(BattleContext::kAbilityLevelSlotCount);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        pool.push_back(i);
    }
    for (int k = 0; k < count && !pool.empty(); ++k) {
        const int pick = ctx->rng().next() % static_cast<int>(pool.size());
        const int stat = pool[static_cast<std::size_t>(pick)];
        pool.erase(pool.begin() + pick);
        (void)g_core->stat_drop(ctx, opp, stat, amount, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1417, effect_1417_no_drop_random_drop)
