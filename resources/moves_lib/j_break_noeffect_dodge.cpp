// J 组 J100 · 消回合双分支/闪避窗族 —— 1777×14 / 991×14（2026-10-03，worktree ../agents/j100）
//
// 家族语义（effect_info 官方模板即权威）：
//   1777（arg_count=2）"消除对手回合类效果，若对手不处于回合类效果则{0}%令对手{1}"
//   991（arg_count=2）"未击败对手则对方下{0}次攻击技能将{1}%被己方闪避"
//
// 实现口径：
//   - 1777 = 2038 双分支同构（j_purge_round 行）：break_round_effects(opp) →
//     **仅 NO_EFFECTS 分支**（对手本就无回合类效果）→ 申报制异常 chance={0}
//     （SUCCESS/IMMUNE 分支无后续——模板"若…则"字面）。
//   - 991 = 未击败门（J12 桥：当回合双态 AFTER_ACTION 探针 duration=1、行动方
//     过滤 + 对手 hp>0）→ NextRounds({0}) ROUND_START tick 抬 dodge_rate {1}%
//     （{0} 回合窗）。⚠️ 事件选型记档："下{0}次攻击"无法 per-attack 抬避——
//     EVENT_SKILL_USED emit 只入队（drain 晚于 roll，实证失效）、
//     BEFORE_SKILL_HIT 锚点每回合仅首动方一次（二动方攻击收不到）→ 按 J18 1204
//     "次≈回合"既定口径取回合窗（{0}=1 载体 10/14 等价"下一回合"）。
//
// 载体切片（unity 库 json_each 精确匹配 14/14=28；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1777 [概率,异常码]：{100,X}×11/{70,80}×2/{70,6}（100 档确定性；
//     场景载体 浅滩浮游 36239 [100,28]——447 兄弟=伤害下限，不扰断言）
//   991 [次数,概率]：{1,100}×10/{1,50}×2/{1,70}/{2,70}
//     （纯链载体 瀚空之诺 36117 [1,100] 特130——100% 闪避单次，确定性档）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1777/991 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：2038（break 双分支，J1）、27021 锚点（EVENT_SKILL_USED 先于 query_usage）、
//   568/J62（dodge_rate 消费面 = skills.cpp 命中 roll）、J12 桥（未击败门）。
// 场景回归：test/scenario/scenario_1910_break_noeffect_dodge.cpp（1910 号段，一文件两宏）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（897/J12/J18 同款小工具，插件各自持有一份）。
int j100_bucket_acting_side(const BattleContext* ctx, int self) {
    if (!ctx) {
        return false;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first == self;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return first != self;
        default:
            return false;
    }
}

// ── 991 的 NextRounds ROUND_START tick —— 下回合整回合抬避 {1}% ──
// ⚠️ 口径记档（J18 1204"次≈回合"同款）："下{0}次攻击"以**回合窗**表达——
// {0}=1 载体（全库 10/14）等价"下一回合"；多段攻击/额外行动同回合共享窗。
// EVENT_SKILL_USED/BEFORE_SKILL_HIT 均无法做 per-attack 抬避（前者 drain 晚于
// roll、后者每回合仅首动方一次），故取回合窗。
EffectResult j100_991_dodge_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int pct = a.int_args[3];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    c->ws.dodge_rate[self] += pct / 100.0f;
    return EffectResult::kOk;
}

// ── 991 的当回合武装探针（duration=1，行动方过滤 + 未击败门）──
EffectResult j100_991_arm_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int attacks = a.int_args[2];
    const int pct = a.int_args[3];
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已击败：门关
    }
    if (!j100_bucket_acting_side(c, self)) {
        return EffectResult::kOk;   // 只在自身行动结束后武装一次（双桶择一）
    }
    register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 991,
                                /*rounds=*/attacks, {self, opp, attacks, pct},
                                &j100_991_dodge_tick);
    return EffectResult::kOk;
}

// ── 1777 —— 消除对手回合类效果，若对手不处于回合类效果则{0}%令对手{1} ──
// int_args = [owner, target, chance, anomaly]（args_num=2）。
EffectResult effect_1777_break_noeffect_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1777, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1777, args,
                     "1777 布局=[owner,target,概率,异常码]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1777, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0, 1777, args, "需要 概率 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1777, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->break_round_effects || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    // 仅 NO_EFFECTS 分支（"若对手不处于回合类效果"字面）；SUCCESS/IMMUNE 无后续。
    if (g_core->break_round_effects(ctx, opp) != BreakResult::NO_EFFECTS) {
        return EffectResult::kOk;
    }
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 991 —— 未击败对手则对方下{0}次攻击技能将{1}%被己方闪避 ──
// int_args = [owner, target, attacks, pct]（args_num=2）。
EffectResult effect_991_dodge_counter(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 991, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 991, args,
                     "991 布局=[owner,target,次数,概率]");
    const int self = args.int_args[0];
    const int fixed_unused = args.int_args[1];
    const int attacks = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && fixed_unused >= 0 && fixed_unused <= 1,
                     991, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(attacks > 0 && pct > 0, 991, args, "需要 次数/概率 > 0");
    // 未击败门（J12 桥）：当回合双态 AFTER_ACTION 探针（duration=1，行动方过滤）。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(991, 0, self, -1,
                     EffectArgs(std::vector<int>{self, args.int_args[1], attacks, pct}),
                     &j100_991_arm_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 991;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1777, effect_1777_break_noeffect_anomaly)
SKILL_EFFECT(991, effect_991_dodge_counter)
