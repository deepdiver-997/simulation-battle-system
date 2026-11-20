// J 组 J270 · 秒杀兜底自强化族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1591 [2] "{0}%秒杀对手，未触发则自身全属性+{1}"
//
// 实现口径（直系先例 j_prob_fallback5.cpp 1318 —— 同族镜像：
//   "{0}%的概率秒杀对手，未触发则令对手全属性-{1}"）：
//   - 时点 = FIRST/SECOND_AFTER_ACTION 双侧一次性探针（duration=1，伤害落地后再判，
//     避免"先秒后伤"乱序与死亡漏斗重入；495/1318 同口径）。
//   - 命中门 = ws.skill_exec_result[self] == HIT（J130 口径）；对手已倒地则不再补刀。
//   - 掷点 = roll_percent({0})；中 → g_core->force_hp_to_zero（秒杀族统一入口：
//     免疫票/短路转化/EVENT_HP_TO_ZERO 全链生效，归因写 last_damage_actor）。
//   - 未中 → all_stats_up(self, {1})：六项能力（攻/特攻/防/特防/速/命中）逐槽
//     stat_change 正增益（自身增益不吃免弱，本体/视图同步）。
//   - 审计槽 991591000 + owner = 1（本次秒杀）/ 0（本次兜底），场景可无随机断言分支。
//   - {0}/{1} 在位即消费（显式 0 不跳过：chance=0 恒走兜底、delta=0 为合法空增益）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1591 [chance, delta]：28997 狡诈童话 [1,1]、34305 黯魍迷音曲 [10,1]、
//     34365 流魅夺命击 [10,1]、34906 枯朽焕生术 [10,1]、34946 学神的祝福 [10,1]、
//     35530 混沌齿轮 [10,1]（纯链，场景载体）、36512 异空绞命光 [10,1]、
//     38260 脑髓地狱 [1,1]（8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1591 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1591) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6010_prob_kill_self_boost.cpp（号段 6010-6019）。

#include "common.h"

namespace {

constexpr int kAuditBase = 991591000;   // +owner：1=秒杀 / 0=兜底

// AFTER_ACTION 桶的当前行动方（j_prob_fallback5 / batch_a 同款）。
int j270_bucket_acting_side(const BattleContext* ctx) {
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

// ── 1591 探针：命中且存活 → roll 秒杀 / 未中 → 自身全属性+{1} ──
EffectResult j270_1591_kill_probe(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !g_core || !g_core->force_hp_to_zero || !args.int_args
        || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int delta = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    if (j270_bucket_acting_side(ctx) != self) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 命中门（J130/1318 口径）
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：不再补刀
    }
    if (!roll_percent(chance)) {
        ctx->plugin_storage[kAuditBase + self] = 0;
        all_stats_up(ctx, self, delta);   // 未触发兜底：自身六槽 +delta
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kAuditBase + self] = 1;
    (void)g_core->force_hp_to_zero(ctx, opp, self);
    return EffectResult::kOk;
}

// ── 1591 —— {0}%秒杀对手，未触发则自身全属性+{1} ──
// int_args = [owner, target, chance, delta]（args_num=2）。
EffectResult effect_1591_prob_kill_self_boost(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1591, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1591, args,
                     "1591 布局=[owner,target,概率,全属性增幅]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int delta = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1
                         && owner != target,
                     1591, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance >= 0 && chance <= 100, 1591, args, "概率须在 [0,100]");
    const std::vector<int> payload{owner, target, chance, delta};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1591, 0, owner, -1, EffectArgs(payload), &j270_1591_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1591;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1591, effect_1591_prob_kill_self_boost)
