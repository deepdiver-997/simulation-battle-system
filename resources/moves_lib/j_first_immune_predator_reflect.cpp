// J 组 J276 · 先手免伤·天敌反伤族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1511 [1] "先出手时免疫当回合受到的攻击伤害，若对手为自身天敌则免疫并反弹给
//   对手造成伤害值{0}%的百分比伤害"
//
// 实现口径：
//   - 先手门 = `ctx->is_second_mover(self)` 为真则直接返回（"先出手时"）。
//   - 免伤段 = 红伤管线 BLOCK 相位闭包（1727/816 同款：BLOCK 拿**归零前 final**；
//     只认 d.defenderId==self、isRed、final>0）。归零 base/afterAdd/afterMul/final。
//   - 天敌门 = `is_predator(ctx, self)`（common.h；对手固有属性克制自身）。
//     反射 = final×{0}% 计算成具体值后走 deal_pink_damage(DamageKind::PERCENT_VALUE)
//     （粉伤·百分比档但 amount 已是数值：走百分比抗性/护罩，不按目标 maxHP 二次换算）。
//   - "当回合"：闭包捕获 arm_round，仅在 `c->roundCount == arm_round` 时响应；
//     窗外保持惰性（不误伤后续回合）。
//   - 审计槽 991511000+owner = 本次免伤量；991511100+owner = 本次反射量。
//   - {0} 在位即消费（显式 0 = 免伤但不反弹）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1511 [pct]：28457 踏蔓先登 [50]（场景载体，链上 610 良性）、29194 震慑怒吼 [50]、
//     33772 释逸绝禁 [50]、34310 星愿微光聚 [50]、34578 彩·幻羽之落 [50]、
//     35538 水火地影 [50]、36493 丰岁百香祝 [100]、38137 断金鳍锋斩 [100]（8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1511 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1511) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6090_first_immune_predator_reflect.cpp（号段 6090-6099）。

#include "common.h"

namespace {

constexpr int kImmuneBase = 991511000;   // +owner：本次免伤量
constexpr int kReflectBase = 991511100;  // +owner：本次反射量

// ── 1511 —— 先出手时免疫当回合攻击伤害；天敌则反射 {0}% 百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_1511_first_immune_predator_reflect(BattleContext* ctx,
                                                       const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1511, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1511, args,
                     "1511 布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1511, args, "owner 越界");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 1511, args, "反弹率须在 [0,100]");
    if (ctx->is_second_mover(self)) {
        return EffectResult::kOk;   // "先出手时"——后手不注册
    }
    ctx->plugin_storage[kImmuneBase + self] = 0;
    ctx->plugin_storage[kReflectBase + self] = 0;
    const int arm_round = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::BLOCK, self, DamageEffectCategory::BLOCK,
        [self, pct, arm_round](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self || !g_core || c->roundCount != arm_round) {
                return;   // 只作用于"当回合"
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;   // 只免疫对手的攻击红伤
            }
            const int attacker = d.attackerId;
            if (attacker < 0 || attacker > 1 || attacker == self) {
                return;
            }
            const int immune_amount = d.final;   // 归零前的伤害值
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            c->plugin_storage[kImmuneBase + self] = immune_amount;
            if (pct <= 0 || !is_predator(c, self)) {
                return;   // 非天敌：只免伤，不反弹
            }
            const int reflect = immune_amount * pct / 100;
            c->plugin_storage[kReflectBase + self] = reflect;
            if (reflect > 0 && g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(c, attacker, reflect,
                                               DamageKind::PERCENT_VALUE, self);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1511, effect_1511_first_immune_predator_reflect)
