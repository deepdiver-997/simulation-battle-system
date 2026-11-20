// J 组 J191 · 固伤递增族 —— 1 宏/9 载体（2026-10-22，worktree ../agents/j191）
//
// 家族语义（effect_info 模板即权威）：
//   1480 [3] 附加{0}点固定伤害，每次使用额外附加{1}点，最高{2}点
//
// 实现口径（heal_drain.cpp 497 落芳天华直系，去掉"天敌翻倍"后缀）：
//   - 计数放 pet.on_stage_storage（**本次上场**槽；下场由引擎清空——"每次使用递增"
//     族默认下场不保留，用户 2026-09-13 实测口径）。
//   - 第 1 次 = {0}，之后每次 +{1}，先按 {2} 封顶再落 fixed_damage（红伤系固伤，
//     吃护盾；与 497 同通道）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1480 [基础,递增,封顶]：33748 极电闪耀 [150,100,350]（纯链，特 140）、
//        33868 尾弋毒刺 [150,50,300]、35949 纵魂牵引 [150,50,300]、
//        36699 祥云祈望 [150,100,350]…（9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1480 无硬编码（仅场景号注释）
// ②custom_effect_overrides/programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册
// ——真缺口。
// 场景回归：test/scenario/scenario_5130_fixed_ramp_1480.cpp（号段 5130-5139）。

#include "common.h"

namespace {

// ── 1480 —— 附加{0}点固定伤害，每次使用额外附加{1}点，最高{2}点 ──
// int_args = [owner, target, 基础, 递增, 封顶]（args_num=3）。
EffectResult effect_1480_growing_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1480, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1480, args,
                     "1480 布局=[owner,target,基础,递增,封顶]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1480, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base > 0 && step >= 0 && cap >= 0, 1480, args,
                     "需要 基础>0 且 递增/封顶 ≥0");
    // 本场已用次数（不含本次）：第 1 次 = base。
    auto& slot = ctx->getPet(owner).on_stage_storage[1480];
    if (!slot.has_value()) {
        slot = 0;
    }
    const int used = any_int_or(slot);
    int amount = base + step * used;
    if (cap > 0 && amount > cap) {
        amount = cap;   // "最高{2}点"——先封顶
    }
    slot = used + 1;
    if (amount > 0 && g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1480, effect_1480_growing_fixed)
