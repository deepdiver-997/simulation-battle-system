// J 组 J60 · 异常门条件输出族 —— 2 宏/38 载体（2026-10-03，worktree ../agents/j60）
//
// 家族语义（effect_info 模板即权威）：
//   552 [2] 若对手处于异常状态，则{0}%概率附加{1}点伤害
//   194 [3] 造成伤害的1/{0}恢复自身体力，若对手{1}，则造成伤害的1/{2}恢复自身体力
//
// 实现口径：
//   - 552 = 异常门 has_any_active_abnormal_status(opp)（1248/1024 同判据，
//     BattleContext 内联）+ 概率固伤 SKILL_EFFECT 直通（93 同款 roll_percent →
//     deal_pink_damage FIXED；工单口径，非申报闸——附加伤害无落地闸可管）。
//   - 194 = 双档吸血读**裸伤台账** ws.raw_attack_damage[owner]（417/628 同通道：
//     挡伤/锁伤改掉的红伤不算；工单口径）。归位 AFTER_ACTION 经内核
//     effect_register_state case 194（本次随本任务加一行，101/422 先例）。
//     分档：对手有异常码 {1}（官方 0..43，含 0=麻痹）→ raw/{2}，否则 raw/{0}；
//     回血走 heal_amount（吃封回血/恢复修正门）。桶只跑行动方（内核路由即然）。
//
// 载体切片（unity 库 json_each 精确匹配，切片全自洽）：
//   552 [概率,固伤]：纯链 19684 破冰狂击怒 [100,300]（100% 档）、15249 灭世轮回击
//        [30,200]（概率档）等 19 条
//   194 [denom,条件码,denom2]：纯链 15185 沙石毒王击 [4,1,2]（中毒档 ÷2）等 19 条
//
// 动工前置三查（2026-10-03）：①内核 grep 552/194 无硬编码（194 原先 default 落
// SKILL_EFFECT 时点=本次归位修复）②custom_effect_overrides 无声明 ③moves_lib
// 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1460_status_gate_output.cpp（号段 1460-1469）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

// ── 552 —— 若对手处于异常状态，则{0}%概率附加{1}点伤害 ──
// int_args = [owner, target, chance, amount]（args_num=2）。
EffectResult effect_552_statused_chance_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 552, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 552, args,
                     "552 布局=[owner,target,概率,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     552, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && amount > 0, 552, args, "需要 概率/固伤 > 0");
    if (!ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 门：对手处于异常状态
    }
    if (roll_percent(ctx->rng(), chance) && g_core && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 194 的 AFTER_ACTION 体 —— 双档吸血（裸伤台账；内核 case 194 归位本时点）──
// int_args = [owner, target, denom, cond_status, denom2]（args_num=3）。
EffectResult effect_194_conditional_lifesteal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int cond_status = args.int_args[3];
    const int denom2 = args.int_args[4];
    if (owner < 0 || owner > 1 || denom <= 0 || denom2 <= 0
        || !is_valid_abnormal_status_id(cond_status)) {
        return EffectResult::kOk;
    }
    const int raw = ctx->ws.raw_attack_damage[owner];   // 裸伤台账（管线前的公式值）
    if (raw <= 0) {
        return EffectResult::kOk;
    }
    const bool cond = g_core->has_active_anomaly
                          ? g_core->has_active_anomaly(ctx, target, cond_status)
                          : false;
    const int healed = cond ? raw / denom2 : raw / denom;
    if (healed > 0) {
        (void)g_core->heal_amount(ctx, owner, healed);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(552, effect_552_statused_chance_fixed)
SKILL_EFFECT(194, effect_194_conditional_lifesteal)
