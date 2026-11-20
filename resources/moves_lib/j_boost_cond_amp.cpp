// J27 · 条件强化输出族 —— 845×21 / 709×21 / 808×22（2026-10-02，worktree ../agents/j27）
//
// 家族语义（effect_info 官方模板），三条都围绕"自身能力提升状态/体力条件"放大输出：
//
// 845（arg_count=2）"若自身当前体力高于最大体力的1/{0}则威力提升{1}%"：
//   - hp（当前）> max_hp/{0}（严格高于，max_hp_of 读 numericalBase——common.h 口径）
//     → 变威力：skill_power_view[自身] += view × {1} / 100（102 通道；view>0 守卫，
//     强制执行打盔的 0 威力不复活；写=声明变威力 → ATTACK_DAMAGE 收尾重算第二次）。
//   - 实例：电暗天威 30848 [2,50]、无天元始破 19976 [2,30]（全库 21 载体，{0} 全为 2）。
// 709（arg_count=0）"击败对手时若自身处于能力提升状态，则将所处的能力提升状态翻倍"：
//   - **AFTER_ACTION 桥反向门**（J12/897 同构双探针：判据取反——对手在场宠 hp<=0
//     =本次攻击击败了它；AFTER_ACTION 跑在死亡处理/死切之前，看到的还是倒下的对手）；
//   - "处于能力提升状态" = 任一正槽 >0（探针时点复判）；"翻倍" = 正槽逐项
//     stat_change(+当前值)（J12 964 同口径：403 取法，AT_CAP 自然封顶；负槽不动）。
//     moves_lib 单 dylib 内匿名空间符号不跨 TU → 964 翻倍循环就地重写（J12 文件
//     在 feat/j12 未并分支）。
//   - 实例：奥义神灭斩 16981（全库 21 载体，全物160 段攻击技）。
// 808（arg_count=1）"自身每处于一种能力提升状态则附加{0}点固定伤害"：
//   - 正槽**种数**（batch_c count_boost_kinds_c 同口径循环）× {0} → fixed_damage
//     （SKILL_EFFECT 时点，1529 通道）。
//   - 实例：霹雳迅击 30952 [80]、明月光喑舞 19931 [60]（全库 22 载体，{0}∈{30..80}）。
//
// 动工前置三查（2026-10-02）：①内核 grep 845/709/808 无行为硬编码 ②effect_meta
// 无分类牵连 ③moves_lib 无既有注册——真缺口。载体数核对：21/21/22；全量切片核验
// 64 载体布局零异常（845 [分母,百分比] / 709 空参 / 808 [点数]）。
// 先例：102/882（变威力）、J12 964（正槽翻倍 + AFTER_ACTION 桥）、403（翻倍取法）、
//   batch_c count_boost_kinds_c、1529（fixed_damage）。
// 场景回归：test/scenario/scenario_1120_boost_amp.cpp（1120 号段，一文件三宏）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（897/J12 同款小工具，插件各自持有一份）。
int j27_bucket_acting_side(const BattleContext* ctx) {
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

// ── 845 —— 若自身当前体力高于最大体力的1/{0}则威力提升{1}% ──
EffectResult effect_845_high_hp_power_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 845, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 845, args,
                     "布局=[owner,target,denom,pct]");
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 845, args, "owner 越界");
    EFFECT_ARG_GUARD(denom > 0 && pct > 0, 845, args, "需要 denom>0 && pct>0");
    const int hp = ctx->getPet(self).hp;
    if (hp <= max_hp_of(ctx, self) / denom) {
        return EffectResult::kOk;   // 前提：当前体力高于 1/{denom}（严格高于）
    }
    const int view = ctx->ws.skill_power_view[self];
    if (view > 0) {
        ctx->ws.skill_power_view[self] = view + view * pct / 100;   // 变威力（102 通道）
    }
    return EffectResult::kOk;
}

// ── 709 共用探针（无捕获 EffectFn）：击败对手且自身有提升 → 正槽逐项翻倍 ──
EffectResult j27_709_kill_double_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->stat_change || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (self < 0 || self > 1 || j27_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判
    }
    if (c->getPet(1 - self).hp > 0) {
        return EffectResult::kOk;   // "击败对手时"门（J12 桥反向）
    }
    bool any_boost = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (c->ability_levels[self][i] > 0) {
            any_boost = true;
        }
    }
    if (!any_boost) {
        return EffectResult::kOk;   // 前提：自身处于能力提升状态
    }
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int lv = c->ability_levels[self][i];
        if (lv > 0) {
            (void)g_core->stat_change(c, self, i, lv);   // 正槽翻倍（J12 964 同口径）
        }
    }
    return EffectResult::kOk;
}

// ── 709 —— 击败对手时若自身处于能力提升状态，则将所处的能力提升状态翻倍（零参）──
EffectResult effect_709_kill_double_boosts(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 709, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 709, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 709, args, "owner 越界");
    const std::vector<int> payload{self, 1 - self};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(709, 0, self, -1, EffectArgs(payload), &j27_709_kill_double_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 709;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 808 —— 自身每处于一种能力提升状态则附加{0}点固定伤害 ──
EffectResult effect_808_per_boost_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 808, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 808, args,
                     "布局=[owner,target,amount]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 808, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(amount > 0, 808, args, "需要 amount>0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    int kinds = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[self][i] > 0) {
            ++kinds;   // 正槽种数（batch_c count_boost_kinds_c 同口径）
        }
    }
    if (kinds <= 0) {
        return EffectResult::kOk;   // 前提：每处于一种（0 种不附加）
    }
    (void)g_core->fixed_damage(ctx, opp, kinds * amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(845, effect_845_high_hp_power_up)
SKILL_EFFECT(709, effect_709_kill_double_boosts)
SKILL_EFFECT(808, effect_808_per_boost_fixed)
