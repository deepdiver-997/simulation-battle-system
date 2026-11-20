// J 组 J226 · 低体下回合先制族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1230 [2] 自身体力低于对手时，下{0}次自身技能先制+{1}
//
// 实现口径（直系先例 j41_second_preempt.cpp 1549）：
//   - 条件 = 自身当前体力 **严格小于** 对手当前体力（SKILL_EFFECT 时点快照）。
//   - 后缀 = NextRounds({0}) 的 BATTLE_FIRST_MOVE_RIGHT tick：
//     `ws.preemptive_level[self] += {1}`（843/499 先制通道；MOVE_RIGHT 时点 memset 后写，
//     下一回合的首手判定即读它）。
//   - "次"≈回合口径（1204 同近似）：一回合至多一次出手；全库 {0} 实载恒为 1
//     （见下），因此与字面"下 1 次"精确等价，不存在跳回合烧窗的歧义。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 9/9 自洽）：1230 [times,prio] 全部 [1,*]：
//   28024 幽心萧瑟 [1,2]（场景载体；链兄弟 191 自免异常 3 回合，无先制/体力面）/
//   28112 [1,2] / 31084 [1,2] / 31473 [1,2] / 32464 [1,2] / 35494 [1,1] /
//   36606 [1,2] / 37276 [1,2] / 37618 [1,2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1230 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口
//   （g_cond_boost.cpp 注释把 1230 标为"未实现链上静默跳过"）。
//
// 场景回归：test/scenario/scenario_5570_low_hp_next_priority.cpp（号段 5570-5579）。

#include "common.h"

namespace {

// ── 1230 tick：下一回合 MOVE_RIGHT 写先制 +{1} ──
EffectResult j226_1230_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[2];
    if (self < 0 || self > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[self] += bonus;
    return EffectResult::kOk;
}

// ── 1230 —— 自身低体时下{0}次自身技能先制+{1} ──
// int_args = [owner, target, times, bonus]（args_num=2 → 装载器注入 owner/target）。
EffectResult effect_1230_low_hp_next_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1230, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1230, args,
                     "1230 布局=[owner,target,times,bonus]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1230,
                     args, "需要 owner/target ∈ {0,1}");
    // 严格小于（模板"低于"）；相等不触发。
    if (ctx->getPet(owner).hp >= ctx->getPet(opp).hp) {
        return EffectResult::kOk;
    }
    // 行参在位即消费：times/bonus 直读，0/负档不注册（不回退默认值）。
    if (times > 0 && bonus > 0) {
        register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1230, times,
                                    {owner, opp, bonus}, &j226_1230_priority_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1230, effect_1230_low_hp_next_priority)
