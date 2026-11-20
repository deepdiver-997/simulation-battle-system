// J129 · 命中掷骰兜底族 —— 1000×12（2026-10-04，worktree ../agents/j129）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1000（args_num=3）"命中则{0}%使对手{1}，未触发则附加{2}点固定伤害"：
//   - 命中门 = AFTER_ACTION 探针读 ws.skill_exec_result==HIT（J23 36 同判据）。
//     ⚠️ 不能在 SKILL_EFFECT 桶里读：write_skill_resolution 在 execute() **返回后**
//     才写结果（battleFsm.cpp resolve_skill_execution 尾部），桶内读到的是回合初
//     reset 的 SKILL_INVALID 默认值——36 用 495 AFTER_ACTION 延迟器正是为此；
//   - 掷骰 = apply_anomaly_checked 申报制（{0}%，ChanceSource::Skill，亮节闸门管辖）；
//     "未触发" = 非 SUCCESS/REPLACED_EXISTING/DURATION_EXTENDED 三档（j85_landed
//     取补——掷歪/免疫都算）→ fixed_damage(对手, {2})（J85 1195 兜底分叉同款）；
//   - 存活门（2295 口径）：本次攻击已打死对手则整条跳过（对尸体施加/补刀无意义）。
//   - 真 miss 在 execute() 内提前 return（skills.cpp 命中判定 MISS 分支），SKILL_EFFECT
//     桶根本不跑 → 探针不注册 → 全链自然不触发，无需额外 miss 判。
//
// 动工前置三查（2026-10-04，立单时已核）：①内核 grep 1000 仅数字噪声
//   （idle_sweep_interval_ms/kSoulShieldPriority）②battle_effects/custom_effect_overrides
//   无行 ③moves_lib 全 worktree 并集无注册。⚠️ 第四查（实测补）：同链未注册效果会被
//   **内核运行时文本 parser**（skills.cpp parse_effect_unit）兜底解析执行——载体须选
//   纯链（37377 链上 917 被 parser 解析活、100% 上寄生，场景实测教训，详见速查）。
//   切片：side_effect.arg_count 1000=3 与
//   effect_info.args_num 一致；12 载体 [概率,异常码,固伤] 自洽零剩余（脚本核算
//   2026-10-04，码表 0/1/2/5/6/7/9/15）。
// 先例：J23 36（AFTER_ACTION 探针+HIT 门+存活门）、J85 1195（申报制+j85_landed
//   三档+兜底固伤）、J70（兜底分叉）。
// 场景回归：test/scenario/scenario_2230_hit_roll_fallback.cpp（2230 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 申报制"落地"三档（j85_landed 同口径；匿名空间不跨 TU，就地重写）。
bool j129_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
}

// 本 AFTER_ACTION 桶属哪一方（J23 j23_bucket_acting_side 同判据，就地重写）。
int j129_bucket_acting_side(const BattleContext* ctx) {
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

// 1000 探针（无捕获 EffectFn，J23 36 同构）：payload = [self, opp, chance, anomaly, fixed]。
EffectResult j129_1000_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !g_core->fixed_damage
        || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int chance = a.int_args[2];
    const int anomaly = a.int_args[3];
    const int fixed = a.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || chance <= 0 || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (j129_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判
    }
    if (c->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中则"门（miss/被盔/被封属=SKILL_INVALID 不触发）
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：整条跳过（2295 存活门）
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        c, opp, anomaly, random_anomaly_duration(), self, chance, ChanceSource::Skill);
    if (j129_landed(r)) {
        return EffectResult::kOk;
    }
    (void)g_core->fixed_damage(c, opp, fixed);   // 未触发 → 附加固伤
    return EffectResult::kOk;
}

}  // namespace

// ── 1000 —— 命中则{0}%使对手{1}，未触发则附加{2}点固定伤害 ──
// int_args = [owner, target, chance, anomaly, fixed]（args_num=3）。
EffectResult effect_1000_hit_roll_fallback(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1000, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1000, args,
                     "1000 布局=[owner,target,概率,异常码,固伤]");
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int fixed = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1000, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && fixed > 0, 1000, args, "需要 概率/固伤 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1000, args, "异常码越界（≤43）");
    // AFTER_ACTION 延迟器（36 同款）：双方桶各挂一条一次性探针，duration=1 本回合。
    const std::vector<int> payload{self, 1 - self, chance, anomaly, fixed};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(1000, 0, self, -1, EffectArgs(payload), &j129_1000_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1000;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1000, effect_1000_hit_roll_fallback)
