// J 组 J164 · 随机三异常族 —— 1 宏/10 载体（2026-10-04，worktree ../agents/j164）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   74（args_num=0）"命中时30%令对手中毒/烧伤/冻伤三选一"
//     —— attach_random_anomalies 限池版（1367 直系）：池手写 {1=中毒, 2=烧伤,
//     5=冻伤}、count=1、probability=30、ChanceSource::Skill（概率闸门管辖）。
//     命中门 = ws.skill_exec_result==HIT（模板"命中时"字面，J23 36 判据）——
//     495/36 AFTER_ACTION 延迟器同款（伤害落地后判，miss/被盔不触发）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 10/10 零参自洽、零剩余）：
//   纯链×8：奇幻光芒 11140 等特技（场景利）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 74 无行为硬编码
//   ②custom_effect_overrides 零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J23 36（AFTER_ACTION 延迟器+HIT 门）、1367（attach_random_anomalies 通道）。
// 场景回归：test/scenario/scenario_2620_random_trio_anomaly.cpp（号段 2620-2629，
//   30% 统计带断言 + 落码池校验）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 74 延迟器 —— AFTER_ACTION：命中门 → 30% 三选一 ──
EffectResult j164_74_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int arm_round = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1
        || arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[self] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // "命中时"门（miss/被盔不触发）
    }
    if (ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].hp <= 0) {
        return EffectResult::kOk;   // 已被本次攻击打死：无需再附（2295 存活门）
    }
    const std::vector<int> pool{static_cast<int>(AbnormalStatusId::Poison),
                                static_cast<int>(AbnormalStatusId::Burn),
                                static_cast<int>(AbnormalStatusId::Frostbite)};
    (void)g_core->attach_random_anomalies(ctx, opp, pool, /*count=*/1,
                                          /*probability_pct=*/30, self,
                                          ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 74 —— 命中时30%令对手中毒/烧伤/冻伤三选一（零参）──
EffectResult effect_74_hit_random_trio(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 74, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 74, args,
                     "74 布局=[owner,target]（零参效果）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 74,
                     args, "需要 owner/target ∈ {0,1}");
    const std::vector<int> payload{self, opp, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(74, 0, self, -1, EffectArgs(payload), &j164_74_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 74;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(74, effect_74_hit_random_trio);
