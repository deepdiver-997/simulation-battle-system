// J 组 J99 · 未击败固伤/反转回满族 —— 838×14 / 778×14（2026-10-03，worktree ../agents/j99）
//
// 家族语义（effect_info 官方模板即权威）：
//   838（arg_count=1）"若当回合未击败对手，则下回合附加{0}点固定伤害"
//   778（arg_count=0）"反转对手的能力提升状态，反转成功则恢复自身全部体力"
//
// 实现口径：
//   - 838 = 956 两相同形（J12/J84）：施放 → 当回合双态 AFTER_ACTION 武装探针
//     （duration=1，行动方过滤，gate=对手 hp>0 即"未击败"）→ NextRounds(1)
//     ROUND_START tick → fixed_damage(opp, {0})。⚠️ 单相（SKILL_EFFECT 直接注册）
//     会被当回合 AFTER_ACTION 窗吃掉——J84 实证，两相是必要形态。
//   - 778 = stat_boost_reversal 门（1110 bailuo 同款原语直呼，零参
//     bind_participants [owner,target]）== REVERSED → heal_max_hp_fraction(1)
//     回满。NOTHING/BLOCKED 均不回。
//
// 载体切片（unity 库 json_each 精确匹配 14/14=28；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   838 [固伤]：{200} 等（纯链 萌神巨力击 18208 [200] 特150 / 血腥之风 35695）
//   778 零参：分散（纯链 天降魔神 17753 物80 / 守护之荣 17973）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 838/778 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：956（两相 AFTER_ACTION+NextRounds，J12/J84）、1110（stat_boost_reversal
//   门+后续，bailuo J17）、heal_max_hp_fraction。
// 场景回归：test/scenario/scenario_1900_undefeat_reversal.cpp（1900 号段，一文件两宏）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（897/J12/J18 同款小工具，插件各自持有一份）。
int j99_bucket_acting_side(const BattleContext* ctx) {
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

// ── 838 的 NextRounds ROUND_START tick —— 下回合开场附加 {0} 固伤 ──
EffectResult j99_838_round_start_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->fixed_damage || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int fixed = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：无附加对象（956 同守卫）
    }
    (void)g_core->fixed_damage(c, opp, fixed);
    return EffectResult::kOk;
}

// ── 838 的当回合武装探针（duration=1，行动方过滤 + 未击败门）──
EffectResult j99_838_arm_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    if (j99_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在自身行动结束后判一次
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // "当回合未击败"门：已击败不武装
    }
    const std::vector<int> payload{self, opp, a.int_args[2]};
    register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 838,
                                /*rounds=*/1, payload, &j99_838_round_start_tick);
    return EffectResult::kOk;
}


// ── 838 —— 若当回合未击败对手，则下回合附加{0}点固定伤害 ──
// int_args = [owner, target, fixed]（args_num=1）。
EffectResult effect_838_undefeat_next_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 838, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 838, args,
                     "838 布局=[owner,target,固伤]");
    const int self = args.int_args[0];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     838, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 838, args, "需要 固伤 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(838, 0, self, -1, EffectArgs({self, args.int_args[1], fixed}),
                     &j99_838_arm_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 838;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 778 —— 反转对手的能力提升状态，反转成功则恢复自身全部体力 ──
// args_num=0：bind_participants → [owner, target]。
EffectResult effect_778_reversal_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 778, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 778, args,
                     "778 布局=[owner,target]（零参 bind_participants）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     778, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_boost_reversal || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, opp) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // "反转成功"门槛（无可反转/被挡 → 不回）
    }
    (void)heal_max_hp_fraction(ctx, owner, 1);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(838, effect_838_undefeat_next_fixed)
SKILL_EFFECT(778, effect_778_reversal_heal)
