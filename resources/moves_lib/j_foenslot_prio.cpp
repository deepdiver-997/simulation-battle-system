// J91 · 对手负槽先制族 —— 1243×15（2026-10-03，worktree ../agents/j91）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1243（args_num=0）"对手处于能力下降状态时先制+1"：
//   - 选择期路由：内核 loadSkills **selection_effects_ 硬编码表**加 1243
//     （J74 747 孪生——条件先制必须走表，effect_register_state case 赶不上首回合，
//     J66 勘误）；零参 → build_effect_args_for_skill 垫 [owner,target]（964 同款）；
//   - 门 = **对手**任一负槽（747 判自身，本条判对手——同 has_any_stat_drop 镜像）
//     → ws.preemptive_level[owner] += 1（零参，先制数写死文案）。
//   - 实例：飞燕无影剑 31122 [828,1243,40]|[]（全零参链：828=自身提升先制+1 已注册
//     e_priority（空场自灭）、40=先手威力×2 已注册 damage_modifier——对先制断言面
//     双良性）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1243 无硬编码（不在 selection 表/case 表）
//   ②effect_meta 无分类牵连 ③moves_lib 全 worktree 并集无注册——真缺口。
//   切片：side_effect 表 arg_count 1243=0 与 effect_info.args_num 一致；15 载体全零参链。
// 内核改动：skills.cpp loadSkills 的 selection_effects_ 路由表加 1243 一项。
// 先例：J74 747（负槽门先制直系，判据取对手侧）、J66 1491（勘误记档）。
// 场景回归：test/scenario/scenario_1800_foenslot_prio.cpp（1800 号段）。

#include "common.h"

namespace {

// 任一负槽（J14 判据；匿名空间不跨 TU，就地重写）。
bool j91_has_any_stat_drop(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 1243 —— 对手处于能力下降状态时先制+1（选择期，selection 表路由到 MOVE_RIGHT）──
// int_args = [owner, target]（args_num=0，bind 补位）。
EffectResult effect_1243_foen_slot_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1243, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1243, args,
                     "1243 布局=[owner,target]（零参补位）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1243, args, "owner 越界");
    if (!j91_has_any_stat_drop(ctx, 1 - owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += 1;
    return EffectResult::kOk;
}

SKILL_EFFECT(1243, effect_1243_foen_slot_priority)
