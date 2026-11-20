// J 组 J136 · 暴击门族 —— 1527×12 / 1465×12 / 461×12（2026-10-04，worktree ../agents/j136）
//
// 家族语义（effect_info 模板即权威）：
//   1527（0 参）"后出手则必定打出致命一击"      —— 蕴灵红壤 35149、雪樱之凋 35223…
//   1465（1 参）"先出手时50%的概率打出致命一击，未触发则令对手全属性-{0}"
//                                              —— 激化胞质体 37938 [1]（纯链）。
//   461 （1 参）"若自身生命值低于1/{0}则从下回合开始必定致命一击"
//                                              —— 疾动旋风击 13926 [3]（纯链物）等×5。
//
// 实现口径（J114 两级注册同款）：
//   - 暴击掷点在 query_usage ①.5（先于 SKILL_EFFECT）→ 本回合当刀的 must_crit 必须在
//     ACTION_START（query_usage 之前、先手权已定）写好。MOVE_RIGHT 路由体（先手权
//     比较前，判不了先后）注册**一次性 ACTION_START tick**：
//     · 1527 → SECOND_ACTION_START tick（后手方时间线专属桶）：!acting_side_is_first
//       → ws.must_crit[owner]=true（58 圣光气同通道；crit_rate==0 技能也"必定"）。
//     · 1465 → FIRST_ACTION_START tick：先手域内 roll_percent(50) → must_crit=true /
//       未触发 → stat_drop 六槽 -{0}（roll 在 tick 内、掷点前——不影响本次红伤 float
//       的 rand 序之外消费，工单口径"先短路后掷骰"不适用：本条 roll 即门）。
//   - 461 = SKILL_EFFECT 低体门（hp×{0} < max_hp 整数直乘、严格低于——1397 口径）→
//     NextRounds(99) ROUND_START tick 每回合重写 must_crit（58 窗口生产通道；官方
//     "从下回合开始"无期限，窗宽 99 记档）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   1527 0 参：纯链×5（35149 蕴灵红壤 特、35223 雪樱之凋 特、35757 防线攻破 物…）
//   1465 [槽码]：37938 激化胞质体 [1]（纯链物）
//   461  [分母]：13926/14566/15895 [3]（纯链物）…
//
// 动工前置三查（2026-10-04）：①内核 grep 1527/1465/461 无硬编码 ②effect_meta：无
// Penetration/路由表牵连（crit 族不在 102 通道）③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2310_crit_gates.cpp（号段 2310-2319）。

#include "common.h"

namespace {

// ── 1527 的 SECOND_ACTION_START 一次性 tick：后手域内写必暴。──
EffectResult j136_second_mover_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// ── 1465 的 FIRST_ACTION_START 一次性 tick：先手掷 50% 必暴 / 未触发全属性降。──
EffectResult j136_first_mover_chance_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int drop = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp || drop <= 0) {
        return EffectResult::kOk;
    }
    if (roll_percent(50)) {
        ctx->ws.must_crit[owner] = true;
        return EffectResult::kOk;
    }
    if (g_core && g_core->stat_drop) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            (void)g_core->stat_drop(ctx, opp, i, drop, owner);
        }
    }
    return EffectResult::kOk;
}

// 路由体（MOVE_RIGHT 桶）：把授予推迟到先手/后手 ACTION_START 一次性 tick。
void j136_register_action_start_tick(BattleContext* ctx, int owner, int effect_id,
                                     State trigger, EffectFn tick,
                                     std::vector<int> payload) {
    (void)register_round_effect(ctx, trigger, owner, effect_id, /*rounds=*/1,
                                std::move(payload), tick);
}

// ── 1527 —— 后出手则必定打出致命一击 ──
EffectResult effect_1527_second_mover_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1527, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1527, args,
                     "1527 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1527, args, "需要 owner ∈ {0,1}");
    j136_register_action_start_tick(ctx, owner, 1527, State::BATTLE_SECOND_ACTION_START,
                                    &j136_second_mover_must_crit, {owner});
    return EffectResult::kOk;
}

// ── 1465 —— 先出手时50%必暴，未触发则令对手全属性-{0} ──
EffectResult effect_1465_first_chance_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1465, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1465, args,
                     "1465 布局=[owner,target,降幅]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int drop = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1465, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(drop > 0, 1465, args, "需要 降幅 > 0");
    j136_register_action_start_tick(ctx, owner, 1465, State::BATTLE_FIRST_ACTION_START,
                                    &j136_first_mover_chance_crit, {owner, opp, drop});
    return EffectResult::kOk;
}

// ── 461 的 ROUND_START tick：窗口期内每回合重写必暴（58 生产通道）。──
EffectResult j136_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// ── 461 —— 若自身生命值低于1/{0}则从下回合开始必定致命一击 ──
EffectResult effect_461_lowhp_persistent_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 461, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 461, args,
                     "461 布局=[owner,target,分母]");
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && denom > 0, 461, args,
                     "需要 owner∈{0,1} 分母>0");
    const int hp = ctx->getPet(owner).hp;
    const int max_hp = max_hp_of(ctx, owner);
    if (hp * denom >= max_hp) {
        return EffectResult::kOk;   // 低体门：hp >= max/{0} 不触发（整数直乘免截断）
    }
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 461,
                                      /*rounds=*/99, std::vector<int>{owner},
                                      &j136_must_crit_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1527, effect_1527_second_mover_crit)
SKILL_EFFECT(1465, effect_1465_first_chance_crit)
SKILL_EFFECT(461, effect_461_lowhp_persistent_crit)
