// J103 · 对手提升先制威力族 —— 539×14（2026-10-03，worktree ../agents/j103）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 539（args_num=0）"对手处于能力提升状态时先制额外+1且威力翻倍"：
//   - 双行为拆两半：
//     ①先制 = selection_effects_ 路由表加 539（J74 747/J91 1243 同款——条件先制必须
//       走表）：body 在 MOVE_RIGHT 判**对手**任一正槽 → preemptive_level+=1；
//     ②威力 = 同 body 再 register_round_effect(rounds=1, SKILL_EFFECT 桶, 同 id) 挂
//       **当回合**节点 → tick（SKILL_EFFECT 态，视图已物化）判对手正槽 →
//       ws.skill_power_view ×2（40 同款写回）。
//       ⚠️ 同回合执行在 J97 878 处是坑、此处是**期望行为**——节点挂在比武装点更晚的
//       SKILL_EFFECT 桶，当回合内正好命中威力计算窗口。
//   - 零参 → build_effect_args_for_skill 垫 [owner,target]（964 同款）。
//   - 实例：驭云赶月 36704 [539]|[]（物130 prio0 纯链——勿用啸天破/夺心箭：
//     priority=2 会污染先制断言，J90 同款坑）。
//
// 动工前置三查（2026-10-03）：①内核 grep 539 无硬编码（不在 selection 表）②effect_meta
//   无分类牵连 ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表
//   arg_count 539=0 与 effect_info.args_num 一致；14 载体全零参。
// 内核改动：skills.cpp loadSkills 的 selection_effects_ 路由表加 539 一项。
// 先例：J74/J91（selection 路由）、J97 878（同回合节点时序——正反两用）、40（视图写回）、
//   J14（正槽判据）。
// 场景回归：test/scenario/scenario_1960_foeboost_prio_power.cpp（1960 号段）。

#include "common.h"

namespace {

// 对手任一正槽（J14 助手镜像；匿名空间不跨 TU，就地重写）。
bool j103_foe_has_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 威力半 tick：对手有正槽 → 视图 ×2（SKILL_EFFECT 态，视图已物化）。
EffectResult j103_power_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (!j103_foe_has_boost(c, 1 - self)) {
        return EffectResult::kOk;
    }
    auto& view = c->ws.skill_power_view[self];
    if (view.value > 0) {
        view = view.value * 2;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 539 —— 对手处于能力提升状态时先制额外+1且威力翻倍（选择期，selection 表路由）──
// int_args = [owner, target]（args_num=0，bind 补位）。
EffectResult effect_539_foeboost_prio_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 539, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 539, args,
                     "539 布局=[owner,target]（零参补位）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 539, args, "owner 越界");
    if (!j103_foe_has_boost(ctx, 1 - owner)) {
        return EffectResult::kOk;   // 门：对手处于能力提升状态
    }
    // ①先制 +1（选择期即时生效）
    ctx->ws.preemptive_level[owner] += 1;
    // ②威力翻倍：挂当回合 SKILL_EFFECT 节点（视图物化后写回）
    const std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    for (const State st :
         {State::BATTLE_FIRST_SKILL_EFFECT, State::BATTLE_SECOND_SKILL_EFFECT}) {
        register_round_effect(ctx, st, owner, 539, /*rounds=*/1, payload,
                              &j103_power_tick);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(539, effect_539_foeboost_prio_power)
