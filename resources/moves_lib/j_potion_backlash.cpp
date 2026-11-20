// J 组 J235 · 药剂反噬族 —— 1 宏/10 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   69 [1] 下{0}回合对手使用体力药剂时效果变成减少相应的体力
//
// 实现口径（一等公民通道，非私造账本）：
//   - 授予 = ctx->grant_potion_backlash(source=owner, source_effect_id=69, target=对手,
//     rounds={0})——rule_center 的 RuleCategory::POTION_BACKLASH 预留位（此前无注册者）。
//   - 查询点唯一 = SeerRobot::use_medicine：带 hp 效果的药剂把"回复 x"改写为"扣 x"
//     （钳 0 可致死；嗑药不是攻击，不触发击败时点效果）。PP/解异常药剂天然不受影响。
//   - 生命周期 = 回合窗口票（register_round=roundCount，window_expired 与
//     is_switch_locked 同比较：current - register >= rounds）；ON_STAGE 作用域，
//     切换/断回合按 RuleCenter epoch 作废。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 10/10 自洽）：69 [rounds]：
//   20314 药剂反噬 [5]（纯链场景载体）/ 27124 [5] / 28383 [5] / 29337 [5] /
//   29386 [4] / 32937 [5] / 34505 [5] / 35499 [5] / 36043 [5] / 36645 [14]。
//
// 动工前置三查（2026-10-22）：①内核 grep 69 无硬编码（POTION_BACKLASH 查询点已存在）
//   ②effect_info 仅本条模板 ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）
//   无注册，且全库无任何 grant_potion_backlash 调用者——真缺口（通道首个注册者）。
//
// 场景回归：test/scenario/scenario_5660_potion_backlash.cpp（号段 5660-5669）。

#include "common.h"

namespace {

// ── 69 —— 下{0}回合对手使用体力药剂时效果变成减少相应的体力 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_69_potion_backlash(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 69, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 69, args,
                     "69 布局=[owner,target,rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 69,
                     args, "需要 owner/target ∈ {0,1}");
    // 行参在位即消费：0/负档 = 不授予窗口（不回退默认值）。
    if (rounds > 0) {
        (void)ctx->grant_potion_backlash(/*source=*/owner, /*source_effect_id=*/69,
                                         /*target=*/opp, /*rounds=*/rounds);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(69, effect_69_potion_backlash)
