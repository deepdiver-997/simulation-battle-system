// J 组 J225 · 命中异常未触发封攻族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1058 [3] 命中则{0}%使对手{1}，未触发则对手{2}回合内攻击技能无效
//
// 实现口径（直系先例 j_hit_roll_fallback.cpp 1000）：
//   - 命中门 = AFTER_ACTION 探针读 ws.skill_exec_result==HIT（1000/J23 36 同判据；
//     SKILL_EFFECT 桶内该结果尚未写入，不能原地读）。
//   - 掷骰 = apply_anomaly_checked 申报制（{0}%，ChanceSource::Skill）；
//     "未触发" = 非 SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED 三档（掷歪/免疫都算）。
//   - 未触发 → seal_skill(attack=true, attribute=false, count=0, duration={2})——
//     burn_trio 1032 同参数形态（可穿透、非命中失效）。
//   - 存活门（2295 口径）：本次已打死对手则整条跳过。
//   - 属性技能也算命中（skills.cpp：属性技能无伤害面但 emit EVENT_HIT/返回 HIT），
//     本族载体全为 cat4，无伤害面，不影响判定。
//
// 载体切片（side_effect.arg_count 优先、effect_info.args_num 兜底，2026-10-22 核算，
// 9/9 自洽）：1058 [pct,anomaly,rounds]：
//   26539 火光烛天 [100,2,1]（100% 档场景载体；链兄弟 1239/1391 无攻击面）/
//   28211 [100,31,2] / 28598 [100,5,2] / 29163 [100,2,1] / 29425 [100,6,1] /
//   38066 [100,2,1] / 25129 [80,2,2]（80% 档统计场景载体）/ 25415 [80,6,2] /
//   26587 [80,6,2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1058 无硬编码 ②effect_info 仅本条模板
//   ③moves_lib 并集（jbatch/kbatch/j186/j198/k347 各 worktree）无注册——真缺口
//   （j_attr_cond_window.cpp 注释把 26539 的 1058 标为 "1058u" = 未实现）。
//
// 场景回归：test/scenario/scenario_5560_hit_anomaly_else_seal_attack.cpp（号段 5560-5569）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（1000/j85_landed 同口径；匿名空间不跨 TU，就地重写）。
bool j225_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 本 AFTER_ACTION 桶属哪一方（1000/j129 同判据，就地重写）。
int j225_bucket_acting_side(const BattleContext* ctx) {
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

// 1058 探针：payload = [self, opp, pct, anomaly, rounds]。
EffectResult j225_1058_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->seal_skill
        || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int pct = a.int_args[2];
    const int anomaly = a.int_args[3];
    const int rounds = a.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp) {
        return EffectResult::kOk;
    }
    if (j225_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判
    }
    if (c->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中则"门（miss/被盔/被封属不触发）
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：整条跳过
    }
    // pct 在位即消费：apply_anomaly_checked 的概率门内处理 0/100 档。
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        c, opp, anomaly, random_anomaly_duration(), self, pct, ChanceSource::Skill);
    if (j225_landed(r)) {
        return EffectResult::kOk;
    }
    // 未触发 → {2} 回合内攻击技能无效（1032 同参：duration 型窗口、可穿透、非命中失效）。
    if (rounds > 0) {
        (void)g_core->seal_skill(c, /*source=*/self, /*target=*/opp, /*effect_id=*/1058,
                                 /*attribute=*/false, /*attack=*/true, /*count=*/0,
                                 /*duration_rounds=*/rounds, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 1058 —— 命中则{0}%使对手{1}，未触发则对手{2}回合内攻击技能无效 ──
// int_args = [owner, target, pct, anomaly, rounds]（args_num=3）。
EffectResult effect_1058_hit_anomaly_else_seal_attack(BattleContext* ctx,
                                                      const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1058, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1058, args,
                     "1058 布局=[owner,target,pct,anomaly,rounds]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1058,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1058, args, "异常码越界（≤43）");
    // AFTER_ACTION 延迟器（1000 同款）：双方桶各挂一条一次性探针，duration=1 本回合。
    const std::vector<int> payload{self, opp, args.int_args[2], anomaly, args.int_args[4]};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1058, 0, self, -1, EffectArgs(payload), &j225_1058_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1058;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1058, effect_1058_hit_anomaly_else_seal_attack)
