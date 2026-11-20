// J23 · 状态条件输出族 —— 97×15 / 500×10 / 36×16（2026-10-02，worktree ../agents/j23）
//
// 家族语义（effect_info 官方模板），三条都是"状态/命中条件的输出放大"：
//
// 97（arg_count=0）"对手处于冻伤状态时，威力翻倍"：
//   - **变威力通道**（102 电能引爆"麻痹→威力翻倍"逐行镜像）：has_active_anomaly(
//     对手, Frostbite) → ws.skill_power_view[自身] = view × 2（view>0 守卫——强制
//     执行打盔的 0 威力不复活；写=声明变威力 → ATTACK_DAMAGE 收尾以新威力重算第二次）。
//   - 实例：永恒冰封 11596、乱舞 12029（全库 15 载体，全是冰系攻技）。
// 500（arg_count=0）"若对手处于害怕状态则伤害翻倍"：
//   - 与 97 的差别在**通道**：模板是"伤害"不是"威力" → ws.damage_add_pct[自身] += 100
//     （776"翻倍"同通道：AMP 加法通道）；判定 has_active_anomaly(对手, Fear)。
//   - 实例：震撼音律 33397、翼卷飓风 34685（全库 10 载体）。
// 36（arg_count=1）"命中时{0}%的概率秒杀对方"：
//   - 495 AFTER_ACTION 延迟器同款（伤害落地后判秒杀，避免"先秒后伤"乱序与死亡漏斗
//     重入）+ 存活门 + roll → force_hp_to_zero（秒杀体系统一落点：免疫票/短路转化/
//     EVENT_HP_TO_ZERO 全在原语内）；"命中"门 = ws.skill_exec_result==HIT（miss=
//     SKILL_INVALID 不触发）。⚠️ 不用 EVENT_HIT watcher：其 emit 在 execute 内、早于
//     本效果的 SKILL_EFFECT 注册（投递先于注册=收不到本次命中事件），实测踩坑记档。
//   - 实例：极度冰点 10210 [100]、绝命火焰 11022 [5]（全库 16 载体，{0}∈{5,6,7,15,100}）。
//
// 动工前置三查（2026-10-02）：①内核 grep 97/500/36 无行为硬编码（命中项为
//   kWeaknessPct{..500} 等数字噪声）②effect_meta 无分类牵连 ③moves_lib 无既有注册
//   （grep 前缀误配 976/973 已排除）——真缺口。载体数核对（EXISTS 口径，json_each
//   文本/数值双表示会使粗扫虚增）：15/10/16；全量切片核验 41 载体布局零异常。
// 先例：102（变威力镜像，batch_b_skills.cpp:543）、776（增伤翻倍）、495（秒杀原语与
//   时点纪律，batch_a_skills.cpp:360）、2295（存活门）、477（EVENT_HIT watcher 注册法）。
// 场景回归：test/scenario/scenario_1080_status_amp.cpp（1080 号段，一文件三宏）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <vector>

namespace {

// ── 97 —— 对手处于冻伤状态时，威力翻倍（零参）──
EffectResult effect_97_frozen_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 97, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 97, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    if (!g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 97, args,
                     "owner/target 越界");
    if (!g_core->has_active_anomaly(ctx, opp,
                                    static_cast<int>(AbnormalStatusId::Frostbite))) {
        return EffectResult::kOk;
    }
    const int view = ctx->ws.skill_power_view[self];
    if (view > 0) {
        ctx->ws.skill_power_view[self] = view * 2;   // 翻倍（写=声明变威力，102 同款）
    }
    return EffectResult::kOk;
}

// ── 500 —— 若对手处于害怕状态则伤害翻倍（零参）──
EffectResult effect_500_fear_damage_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 500, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 500, args,
                     "args_num=0：bind_participants 仍应带 [owner,target]");
    if (!g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 500, args,
                     "owner/target 越界");
    if (!g_core->has_active_anomaly(ctx, opp,
                                    static_cast<int>(AbnormalStatusId::Fear))) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[self] += 100.0f;   // "伤害翻倍"= 通用增伤 +100%（776 同通道）
    return EffectResult::kOk;
}

int j23_bucket_acting_side(const BattleContext* ctx) {
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

// ── 36 共用探针（无捕获 EffectFn，495 同构）：自身行动结束时点判命中 → roll 秒杀 ──
EffectResult j23_36_kill_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->force_hp_to_zero || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int pct = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (j23_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"判
    }
    // "命中时"门：本回合技能执行结果（miss=SKILL_INVALID 不触发）。
    if (c->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：无需再秒（2295 存活门）
    }
    if (roll_percent(pct)) {
        (void)g_core->force_hp_to_zero(c, opp, self);
    }
    return EffectResult::kOk;
}

// ── 36 —— 命中时{0}%的概率秒杀对方 ──
EffectResult effect_36_hit_chance_kill(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 36, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 36, args,
                     "布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 36, args, "owner 越界");
    EFFECT_ARG_GUARD(pct > 0, 36, args, "需要 pct>0");
    // 495 AFTER_ACTION 延迟器同款（伤害落地后判秒杀，避免"先秒后伤"乱序）；
    // 命中门读 ws.skill_exec_result（EVENT_HIT 的 emit 在 execute 内、早于本效果注册，
    // watcher 收不到本次命中事件——投递先于注册，不用事件面）。
    const std::vector<int> payload{self, 1 - self, pct};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(36, 0, self, -1, EffectArgs(payload), &j23_36_kill_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 36;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(97, effect_97_frozen_power_double)
SKILL_EFFECT(500, effect_500_fear_damage_double)
SKILL_EFFECT(36, effect_36_hit_chance_kill)
