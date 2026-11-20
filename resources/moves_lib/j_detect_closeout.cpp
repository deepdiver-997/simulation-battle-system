// J 组 J143 · DETECT 收尾族 —— 1173×11 / 172×10 / 689×10（2026-10-04，
// worktree ../agents/j143）
//
// 家族语义（effect_info 模板即权威）：
//   1173（args_num=1）"未击败对手则回合结束后附加对手最大体力1/{0}的百分比伤害
//   （自身体力为0时也可触发）" —— 洗涤尘埃 34057 [905,1173]|[?,3] 等 11 链。
//   172 （args_num=1）"若后出手，则给予对方损伤的1/{0}会恢复自己的体力"
//     —— 野性冲击 12916 [3]（纯链物）×8。
//   689 （args_num=2）"若造成的伤害高于{0}，则恢复自身1/{1}最大体力"
//     —— 蓝色魔石 31187 [200,4]（纯链物）。
//
// 实现口径（989/541/1415 DETECT 直系三连）：
//   - 1173 = ROUND_END 一次性 tick（register_round_effect rounds=1，当回合末）：对手
//     在场宠存活（hp>0=「未击败」）→ deal_pink(PERCENT_VALUE, max_hp_of(opp)/{0})。
//     「自身体力为0时也可触发」= 不设自身存活门槛（tick 照跑）——濒死横尸技口径，
//     死亡时点是否清 own 回合效果属内核行为，场景实测记档。
//   - 172 = DETECT（bucket==owner、红伤、final>0）+ 后手门（!acting_side_is_first）
//     → heal_amount(自身, final/{0})（整数直除取整，4399 "损伤的1/3恢复"字面）。
//   - 689 = DETECT + final **>** {0}（严格高于）→ heal_max_hp_fraction(自身, {1})
//     （回 1/{1} 最大体力）。
//
// 动工前置三查（2026-10-04）：①内核 grep 1173/172/689 无硬编码 ②effect_meta 无分类
// 牵连 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2360_detect_closeout.cpp（号段 2360-2369）。

#include "common.h"

namespace {

// ── 1173 的 ROUND_END tick —— payload={owner, target, denom}。
EffectResult j143_undefeat_pink_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || owner == target || denom <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& opp_pet = ctx->getPet(target);
    if (opp_pet.hp <= 0) {
        return EffectResult::kOk;   // 「未击败对手」门：对手横尸则无事发生
    }
    const int pink = max_hp_of(ctx, target) / denom;
    if (pink > 0) {
        // 「自身体力为0时也可触发」：不查自身存活（横尸技口径，见头注）
        (void)g_core->deal_pink_damage(ctx, target, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1173 —— 未击败对手则回合结束后附加对手最大体力1/{0}的百分比伤害 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_1173_undefeat_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1173, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1173, args,
                     "1173 布局=[owner,target,denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1173, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 1173, args, "需要 denom > 0");
    (void)register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1173, /*rounds=*/1,
                                std::vector<int>{owner, target, denom},
                                &j143_undefeat_pink_tick);
    return EffectResult::kOk;
}

// ── 172 —— 若后出手，则给予对方损伤的1/{0}会恢复自己的体力 ──
// int_args = [owner, target, denom]（args_num=1）。
EffectResult effect_172_second_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 172, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 172, args,
                     "172 布局=[owner,target,denom]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     172, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 172, args, "需要 denom > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    // ⚠️ 后手门在**注册时**求值（SKILL_EFFECT 状态在 FIRST/SECOND 域，currentState 即
    // 出手侧——J17 判据）；DETECT 回调运行在伤害管线内，currentState 已不在 FIRST 域，
    // 回调里现查会恒"非先手"（实测翻车点）。
    const bool cast_second = !acting_side_is_first(ctx);
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, denom, cast_second](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            if (!cast_second) {
                return;   // 后手门（注册时烘焙）
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            (void)g_core->heal_amount(c, owner, d.final / denom);
        });
    return EffectResult::kOk;
}

// ── 689 —— 若造成的伤害高于{0}，则恢复自身1/{1}最大体力 ──
// int_args = [owner, target, 阈值, 分母]（args_num=2）。
EffectResult effect_689_high_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 689, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 689, args,
                     "689 布局=[owner,target,阈值,分母]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     689, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && denom > 0, 689, args, "需要 阈值/分母 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            if (d.final <= threshold) {
                return;   // 「高于{0}」严格高于
            }
            (void)heal_max_hp_fraction(c, owner, denom);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1173, effect_1173_undefeat_pink)
SKILL_EFFECT(172, effect_172_second_damage_heal)
SKILL_EFFECT(689, effect_689_high_damage_heal)
