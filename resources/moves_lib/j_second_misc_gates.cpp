// J 组 J145 · 后手/兜底杂门 —— 3 宏/35 载体（2026-10-04，worktree ../agents/j145）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1736 [extra]    "后出手时令对手当回合使用的技能的PP值额外-{0}"
//     —— 后手门（is_second_mover，AFTER_ACTION 时先手权已结算）+ 对手成功用技门
//     （executed_skill_this_round，判据统一 §四点五）→ **定点**扣对手当回合技槽 PP：
//     槽位=roundChoice[opp][1]（工单口径；替换技边缘不覆盖记档），直写 sk.pp
//     （pp=-1 无限 PP 跳过——J68 守卫口径；钳 0）。pp_reduce 是全技能减、不适用。
//   1201 [chance,anomaly,rounds,preempt] "先出手时{0}%使对手{1}，未触发则下{2}回合
//     自身先制+{3}" —— 先手门（SKILL_EFFECT 域 acting_side_is_first，先短路后掷骰）
//     + apply_anomaly_checked 申报制；未触发（j14_landed 三档取补：掷歪/免票都算）
//     → register_next_rounds_effect(FIRST_MOVE_RIGHT, rounds={2}) tick
//     ws.preemptive_level[owner]+={3}（1123 直系；MOVE_RIGHT 桶两侧都跑
//     battleFsm:1704-1705，memset 后、比较前——唯一定时点）。
//   1318 [chance,drop] "{0}%的概率秒杀对手，未触发则令对手全属性-{1}"
//     —— 495 AFTER_ACTION 延迟器直系参数泛化（伤害落地后判秒杀，避免先秒后伤乱序；
//     存活门）：roll_percent({0}) 命中→force_hp_to_zero（秒杀体系统一落点）；
//     未触发（roll 歪——"未触发"=概率未中，记档：免秒杀票挡下的归类不做）→
//     对手六槽（含命中）逐项 stat_drop {1} 级（各查免弱、各钳 -6，749/938 同款）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 12+12+11=35 全载体自洽、零剩余）：
//   1736 [extra] ×12：[2]×6、[4]×2、[5]×2、[20]、[3]
//     纯链：聚能爆雷爪 35489 [2]、泥石吞没 35548 [2]、百舸争流 36654 [4]、
//     幻彩焦点 29041 [20]（属性技）、销魂弑生决 37782 [5]
//   1201 [chance,异常码,回合,先制] ×12：[100,0,2,1]×2、[100,7/8,2,1]×2、
//     [80,…]×5、[70,…]×2、[40,0,2,1]
//     纯链：九霄雷鸣诀 30898 [100,0,2,1]（物150，主干确定档）
//   1318 [chance,drop] ×11：[5,1]×4、[10,1]×4、[6,1]×2、[1,1]
//     纯链：星河洞天舞 33416 [10,1]（特160，掷点校准载体）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 3 id 无硬编码 ②battle_effects
//   无行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：1123/868（MOVE_RIGHT NextRounds 先制 tick）、495/J130-691（AFTER_ACTION 延迟器
//   秒杀）、749/938（全属性逐槽 stat_drop）、J68（pp>=0 守卫）、判据统一 §四点五。
// 场景回归：test/scenario/scenario_2380_second_misc_gates.cpp（号段 2380-2389）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（j14_landed 同口径）。
bool j145_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 当前桶属于哪一侧的出手流程（batch_a 495 同款：先手方由 preemptive_right 解析
// ——效果执行时 FSM 状态可能已推进到对侧桶名，不能按 FIRST/SECOND 字面映射）。
int j145_bucket_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

// ── 1736 tick —— 后手 → 对手当回合技槽 PP 额外扣 ──
EffectResult j145_1736_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int extra = args.int_args[2];
    if (!ctx->is_second_mover(self)) {
        return EffectResult::kOk;   // 后手门（先手无事发生）
    }
    if (!executed_skill_this_round(ctx, opp)) {
        return EffectResult::kOk;   // 对手当回合没用技（被控/NONE 不算）
    }
    const int slot = ctx->roundChoice[opp][1];
    if (slot < 0 || slot >= 5) {
        return EffectResult::kOk;
    }
    Skills& sk = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].skills[slot];
    if (sk.pp >= 0) {
        sk.pp = std::max(0, sk.pp - extra);   // pp=-1 无限 PP 不动（J68 守卫口径）
    }
    return EffectResult::kOk;
}

// ── 1736 —— 后出手时令对手当回合使用的技能的PP值额外-{0} ──
EffectResult effect_1736_second_extra_pp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1736, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1736, args,
                     "1736 布局=[owner,target,extra]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int extra = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1736,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(extra > 0, 1736, args, "需要 extra > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1736, /*rounds=*/1, {self, opp, extra},
                              &j145_1736_tick);
    }
    return EffectResult::kOk;
}

// ── 1201 tick —— MOVE_RIGHT：先制+bonus（1123 直系）──
EffectResult j145_1201_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// ── 1201 —— 先出手时{0}%使对手{1}，未触发则下{2}回合自身先制+{3} ──
EffectResult effect_1201_first_roll_preempt_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1201, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1201, args,
                     "1201 布局=[owner,target,chance,anomaly,rounds,preempt]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    const int preempt = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1201,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && rounds > 0 && preempt > 0, 1201, args,
                     "需要 概率/回合/先制 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1201, args, "异常码越界（≤43）");
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门（先短路后掷骰）
    }
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    if (j145_landed(r)) {
        return EffectResult::kOk;   // 主干落地 → 无兜底
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1201, rounds,
                                {owner, preempt}, &j145_1201_tick);
    return EffectResult::kOk;
}

// ── 1318 延迟器 —— AFTER_ACTION：roll 秒杀 / 歪 → 全属性降 ──
EffectResult j145_1318_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->force_hp_to_zero || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int drop = args.int_args[3];
    const int arm_round = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || arm_round != ctx->roundCount || j145_bucket_side(ctx) != owner) {
        return EffectResult::kOk;
    }
    if (ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：无需再判（2295 存活门）
    }
    if (roll_skill_percent(*ctx, 1318, 1, owner, opp, chance)) {
        (void)g_core->force_hp_to_zero(ctx, opp, owner);
        return EffectResult::kOk;
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        (void)g_core->stat_drop(ctx, opp, i, drop, owner);   // 六槽含命中，各查免弱/钳-6
    }
    return EffectResult::kOk;
}

// ── 1318 —— {0}%的概率秒杀对手，未触发则令对手全属性-{1} ──
EffectResult effect_1318_chance_kill_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1318, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1318, args,
                     "1318 布局=[owner,target,chance,drop]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int drop = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1318,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && chance <= 100 && drop > 0, 1318, args,
                     "需要 概率 ∈ (0,100] 且 降级 > 0");
    const std::vector<int> payload{owner, opp, chance, drop, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1318, 0, owner, -1, EffectArgs(payload), &j145_1318_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1318;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1736, effect_1736_second_extra_pp);
SKILL_EFFECT(1201, effect_1201_first_roll_preempt_fallback);
SKILL_EFFECT(1318, effect_1318_chance_kill_drop);
