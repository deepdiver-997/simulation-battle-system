// J83 · 先手改档族 —— 122×15（2026-10-03，worktree ../agents/j83）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 122（args_num=3）"先出手时，{1}%改变对方{0}等级{2}"：
//   - 先手门 = SKILL_EFFECT 时点先手权直判（J82 138 勘误同款：owner 即出招方，
//     AFTER_ACTION 桶小工具无此态）→ roll_percent({1}) → 改档：
//     {2} ≥ 0 → stat_change(对方, {0}, {2})；{2} < 0 → stat_drop(对方, {0}, |{2}|, 自)
//     （负值=弱化，查免弱，J48 分支口径）；
//   - ⚠️ {0} = **引擎槽直读**（切片 [5,50,-2]=命中——槽 5 只有引擎序才有，无语义序
//     映射；与 148 镜像族的 [stat] 直读口径一致）。
//   - 实例：天际之光 [122]|[5,50,-2]、利爪贯骨击 15393 [122,148]|[1,100,-1,1,100,-2]
//     （148=镜像后手版，已注册 batch_b——双门互补，场景双断言）。
//
// 动工前置三查（2026-10-03）：①内核 grep 122 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   122=3 与 effect_info.args_num 一致；15 载体切片自洽。
// 先例：J82 138（先手门勘误）、J48（stat 分支口径）、batch_b 148（镜像族已注册）。
// 场景回归：test/scenario/scenario_1730_first_slot_change.cpp（1730 号段）。

#include "common.h"

namespace {

}  // namespace

// ── 122 —— 先出手时，{1}%改变对方{0}等级{2} ──
// int_args = [owner, target, slot, chance, delta]（args_num=3）。
EffectResult effect_122_first_slot_change(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 122, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 122, args,
                     "122 布局=[owner,target,slot,chance,delta]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slot = args.int_args[2];
    const int chance = args.int_args[3];
    const int delta = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     122, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 122, args,
                     "槽位越界");
    EFFECT_ARG_GUARD(chance > 0 && delta != 0, 122, args, "需要 chance>0 且 delta≠0");
    if (!g_core) {
        return EffectResult::kOk;
    }
    // 先手门：SKILL_EFFECT 时点先手权已定（owner 即出招方）；后手不触发（148 镜像族管）。
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    if (owner != first || !roll_percent(ctx->rng(), chance)) {
        return EffectResult::kOk;
    }
    if (delta < 0) {
        if (g_core->stat_drop) {
            (void)g_core->stat_drop(ctx, target, slot, -delta, owner);
        }
    } else if (g_core->stat_change) {
        (void)g_core->stat_change(ctx, target, slot, delta);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(122, effect_122_first_slot_change)
