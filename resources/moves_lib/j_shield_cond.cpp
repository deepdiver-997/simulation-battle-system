// J26 · 护盾/护罩条件族 —— 1342×21 / 1120×21 / 1008×21（2026-10-02，worktree ../agents/j26）
//
// 家族语义（effect_info 官方模板），三条都往自己的两本护盾账上记：
//
// 1342（arg_count=1）"为自身附加{0}点护罩"：
//   - **护罩**（hood_bank_，只响应粉伤 FIXED/PERCENT——elf-pet.h 双账注释）≠ 护盾
//     （shield_bank_，只响应红伤 NORMAL）。1853/2418 hood 同款：add(priority=0,
//     amount, source_id, duration=0)。
//   - 实例：生物护盾 27331 [400]、如梦令 28284 [300]（全库 21 载体，属性技载体为主）。
// 1120（arg_count=2）"造成的伤害低于{0}则为自身附加{1}点护盾"：
// 1008（arg_count=2）"造成的伤害高于{0}则获得{1}点护盾"：
//   - **J15 729/1088 DETECT 攻方桶直系**（j_damage_threshold.cpp 同构）：register_
//     damage_effect(DETECT, self, DETECT) 回调读 resolvedDamage——红伤（isRed，属性技
//     无伤害相位自然不触发）、d.attackerId==自身；阈值判 **严格**低于/高于；满足 →
//     shield_bank_.add(0, {1}, source_id, 0)（2005 同款；DETECT 时点加盾 = J3 1278/1727
//     BLOCK 闭包加票同类的管线内副作用先例）。恒久回调（模板无窗口词）。
//   - 实例：八荒岩爆 30454 [280,200]、源力光波 31498 [200,200]（各 21 载体）。
//
// 动工前置三查（2026-10-02）：①内核 grep 1342/1120/1008 无行为硬编码（两本账是
//   通用容器）②effect_meta 无分类牵连 ③moves_lib 无既有注册——真缺口。载体数核对：
//   21/21/21；全量切片核验 63 载体布局零异常（1342 [amount] / 1120、1008 [阈值,数量]）。
// 先例：1853/2418（hood_bank_ add）、2005（shield_bank_ add）、J15 729/1088（DETECT
//   攻方桶阈值判）、816/J3 1278（管线内副作用纪律）。
// 场景回归：test/scenario/scenario_1110_shield_cond.cpp（1110 号段，一文件三宏）。

#include "common.h"

namespace {

// ── 1342 —— 为自身附加{0}点护罩 ──
EffectResult effect_1342_add_hood(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1342, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1342, args,
                     "布局=[owner,target,amount]");
    const int self = args.int_args[0];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1342, args, "owner 越界");
    EFFECT_ARG_GUARD(amount > 0, 1342, args, "需要 amount>0");
    ctx->getPet(self).hood_bank_.add(/*priority=*/0, amount, /*source_id=*/1342,
                                     /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1120 —— 造成的伤害低于{0}则为自身附加{1}点护盾 ──
EffectResult effect_1120_low_dmg_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1120, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1120, args,
                     "布局=[owner,target,threshold,amount]");
    const int self = args.int_args[0];
    const int threshold = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1120, args, "owner 越界");
    EFFECT_ARG_GUARD(threshold > 0 && amount > 0, 1120, args, "需要 阈值/数量 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, threshold, amount](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final >= threshold) {
                return;   // 红伤且严格低于阈值
            }
            c->getPet(self).shield_bank_.add(/*priority=*/0, amount, /*source_id=*/1120,
                                             /*duration=*/0);
        });
    return EffectResult::kOk;
}

// ── 1008 —— 造成的伤害高于{0}则获得{1}点护盾 ──
EffectResult effect_1008_high_dmg_shield(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1008, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1008, args,
                     "布局=[owner,target,threshold,amount]");
    const int self = args.int_args[0];
    const int threshold = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1008, args, "owner 越界");
    EFFECT_ARG_GUARD(threshold > 0 && amount > 0, 1008, args, "需要 阈值/数量 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, threshold, amount](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final <= threshold) {
                return;   // 红伤且严格高于阈值
            }
            c->getPet(self).shield_bank_.add(/*priority=*/0, amount, /*source_id=*/1008,
                                             /*duration=*/0);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1342, effect_1342_add_hood)
SKILL_EFFECT(1120, effect_1120_low_dmg_shield)
SKILL_EFFECT(1008, effect_1008_high_dmg_shield)
