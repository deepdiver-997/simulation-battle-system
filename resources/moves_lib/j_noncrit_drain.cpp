// J139 · 非暴击吸取族 —— 844×12（2026-10-04，worktree ../agents/j139）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 844（args_num=1）"若攻击未打出致命一击则吸取对手最大体力的1/{0}"：
//   - J18 1483 双态 AFTER_ACTION 探针直系：行动方==自身 且 **!crit_happened[自身]**
//     （skills.cpp query_usage 掷、每次技能使用先清、miss 不残留）→
//     drain_hp(自身, 对手, {0})——对手掉最大体力 1/{0}、自身回等量（1088 同原语）；
//   - HIT 门（记档）：模板无"命中"字样，但 miss 时伤害未落地且 crit_happened 已清，
//     "攻击未打出致命一击"的字面在 miss 场景语义不明——取 36 同判据
//     ws.skill_exec_result==HIT 收紧（miss 不吸取），实现头记档待实测纠偏；
//   - 存活门（2295）：本次攻击已打死对手则跳过（吸尸体无意义）。
//
// 动工前置三查（2026-10-04，立单时已核）：①内核 grep 844 无行为硬编码
//   ②battle_effects/custom_effect_overrides 无行 ③moves_lib 全 worktree 并集无注册。
//   载体面：12 载体**同链全实现**（无纯链——496 暴击回满/1002 异常先制/976 消回等，
//   全部良性可避开），切片 [分母] 自洽 {0}∈{3,4}。
// 先例：J18 702/1483（AFTER_ACTION 探针 + crit_happened 判）、J15 1088（drain_hp）、
//   J129 1000（本夜同构探针骨架：双态注册+HIT 门+存活门）。
// 场景回归：test/scenario/scenario_2250_noncrit_drain.cpp（2250 号段）。

#include "common.h"

namespace {

// 本 AFTER_ACTION 桶属哪一方（J23/J129 同判据，匿名空间就地重写）。
int j139_bucket_acting_side(const BattleContext* ctx) {
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

// 844 探针（无捕获 EffectFn）：payload = [self, opp, denom]。
EffectResult j139_844_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->drain_hp || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int denom = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (j139_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在自己出手流程结束判
    }
    if (c->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 命中门（记档：miss 不吸取）
    }
    if (c->crit_happened[self]) {
        return EffectResult::kOk;   // "未打出致命一击"门：暴击了不吸
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：跳过（2295 存活门）
    }
    (void)g_core->drain_hp(c, /*actor=*/self, /*target=*/opp, denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 844 —— 若攻击未打出致命一击则吸取对手最大体力的1/{0} ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_844_noncrit_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 844, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 844, args,
                     "844 布局=[owner,target,分母]");
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     844, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 844, args, "需要 分母 > 0");
    // 双态 AFTER_ACTION 一次性探针（J18 1483/J129 1000 同款：duration=1 本回合）。
    const std::vector<int> payload{self, 1 - self, denom};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(844, 0, self, -1, EffectArgs(payload), &j139_844_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 844;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(844, effect_844_noncrit_drain)
