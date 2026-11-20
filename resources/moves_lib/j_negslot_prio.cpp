// J74 · 负槽门先制族 —— 747×16（2026-10-03，worktree ../agents/j74）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 747（args_num=1）"自身处于能力下降时先制+{0}"：
//   - 选择期路由：内核 loadSkills **selection_effects_ 硬编码表**加 747
//     （J66 1491 勘误直系——effect_register_state case 路由注册进用技期分支，
//     赶不上首回合先制判定，条件先制族必须走本表）；
//   - 门 = 自身任一负槽（ability_levels[owner][i] < 0，J14 has_any_stat_drop 同判据，
//     六槽含命中）→ ws.preemptive_level[owner] += {0}（2020/2298 同款直写）。
//   - 实例：千古幻灭决 18385 [747]|[1]（特150 纯链）、轻烟冉冉 26462 [747]|[1]（属性技）。
//
// 动工前置三查（2026-10-03）：①内核 grep 747 无硬编码（不在 selection 表/case 表）
//   ②effect_meta 无分类牵连 ③moves_lib 全 worktree 并集无注册——真缺口。
//   切片：side_effect 表 arg_count 747=1 与 effect_info.args_num 一致；16 载体切片自洽。
// 内核改动：skills.cpp loadSkills 的 selection_effects_ 路由表加 747 一项
//   （2298/779/1860/1491 同款；与并行任务错峰动 skills.cpp）。
// 先例：J66 1491（selection 路由 + 勘误记档）、J14（负槽判据）、2020（preemptive_level 直写）。
// 场景回归：test/scenario/scenario_1640_negslot_prio.cpp（1640 号段）。

#include "common.h"

namespace {

// 自身任一负槽（六槽含命中，J14 同判据；匿名空间不跨 TU，就地重写）。
bool j74_has_any_stat_drop(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 747 —— 自身处于能力下降时先制+{0}（选择期，selection 表路由到 MOVE_RIGHT）──
// int_args = [owner, target, prio]（args_num=1）。
EffectResult effect_747_negslot_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 747, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 747, args,
                     "747 布局=[owner,target,prio]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 747, args, "owner 越界");
    EFFECT_ARG_GUARD(args.int_args[2] > 0, 747, args, "需要 prio>0");
    if (!j74_has_any_stat_drop(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += args.int_args[2];
    return EffectResult::kOk;
}

SKILL_EFFECT(747, effect_747_negslot_priority)
