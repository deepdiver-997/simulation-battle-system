// F 组批产 · F3 —— 吸取成功族二批：985×36 / 1062×33 / 1214×34（2026-09-29，worktree ../agents/f3）
//
// 家族门槛（1123 直系同款，batch_a_skills.cpp）：**吸取成功 = transfer_stat_boosts(ctx, opp,
// self) > 0**（返回搬走项数；0 = 对手无能力提升 / 被免消除强化挡下 → 后续子句全不给）。
//
// 985（args_num=1）"吸取对手能力提升状态，吸取成功则下{0}回合对手属性技能无效"：
//   - 载体：挟天震世令 19746（[985,986]/[1,50]）、怒炎岂冰决 30021（[985,719]/[1,100,5]）
//     ——两处切片都是 [1]。
//   - "下{0}回合"= NextRounds（官方 §五.0：起点天然 R+1，不叠先后手顺延）→ 1091 同款
//     形态：register_next_rounds_effect（ROUND_START 桶，窗口 1 回合）在 R+1 回合开始时
//     **一次性授予** duration={0} 的回合型封属票。为什么绕一拍：封属票在每回合末
//     tick_rounds 递减（battleFsm ROUND_REDUCTION），若在 R 当回合直授 duration=N，
//     R 回合末就掉一格、R+1 只剩 N-1 回合（还多覆盖 R 剩余时段）；R+1 授 duration=N
//     恰好覆盖 [R+1, R+N]。
//   - 封属形态 = seal_skill(attribute=true, attack=false, count=0, duration={0},
//     hit_invalid=false)——与 695/936"属性技能无效"同字面（普通 SEAL_ATTRIBUTE：技能按
//     SKILL_INVALID 处理、触发无效补偿）；781 的"命中效果失效"（hit_invalid=true）是
//     秩序之助专属口径，不外推。回合型票 count 无意义（内核 duration>0 分支固定传 0）。
// 1062（args_num=1）"吸取对手能力提升状态，吸取成功则吸取对手最大体力的1/{0}"：
//   - 载体：海海之心 28415（[1062,1068,1089]/[4,…]→1/4）、秘月轮世破 30095
//     （[1062,67]/[3,3]→1/3）。
//   - 走 **drain_hp 原语**（内核读 numericalBase[HP] 算 max_hp/denom，目标吃 FIXED 固定
//     伤害、自身 heal_impl 回等量——封回血生效，840/597/1257 同通道）。不手搓
//     max_hp/N + 双原语：drain_hp 的"吸取"语义（含臣服互斥等）集中在内核一处。
// 1214（args_num=1）"对手处于异常状态则吸取对手{0}点体力"：
//   - 载体：雷丝编笼 29035（[48,47,1214,974]/[…,300,…]）、蜃影幽茫剑 30978
//     （[769,1214]/[100,300]）——两处切片都是 [300]：**固定 300 点**，不按异常数放大
//     （args_num=1 + 模板字面"吸取对手{0}点体力"单参数，4399 图鉴第五技描述缺、
//     effect_info 模板即权威）。
//   - 门槛 = 对手处于**任意**异常：遍历官方异常码 0..43 查 g_core->has_active_anomaly
//     （插件条件查询面，自动吃"视为不处于异常"虚拟视图 K6 口径）。注意本条与消强/
//     消回合无关—— abnormal 在身即触发。
//   - 吸取落点：deal_pink_damage(FIXED) 打对手 + heal_amount 等量回自身（840 先例同款
//     双原语形态；drain_hp_amount 固定量版未暴露进 CoreApi，暴露后可换装）。恢复为
//     字面口径：粉伤被护罩全吃也照回（内核 drain 系同样无条件 heal_impl）。
//
// 前置三查（2026-09-29）：①内核无 985/1062/1214 硬编码（grep src/ include/ 无命中）
// ②custom_effect_overrides 无 window 声明（默认 InRounds 家族，"下{N}回合"字面按
// §五.0 用显式 NextRounds，1123/481/681 同策略）③moves_lib 无既有实现。
//
// 场景回归：test/scenario/scenario_630_drain_family.cpp（630-639 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 985 第二跳：R+1 ROUND_START 一次性授予"下{0}回合封属"票（1091 同款形态）────
EffectResult effect_985_seal_grant_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 985, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 985, args,
                     "需要 [owner, target, rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner
                         && rounds > 0, 985, args, "需要 owner/target ∈ {0,1} 且 rounds > 0");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // 回合型封属票：挂施放方（生命周期锚）、生效方 = 对手；duration={0} 恰好覆盖
    // [授予回合, 授予回合+{0}-1]（每回合末 tick 递减）。
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/985,
                       /*attribute=*/true, /*attack=*/false, /*count=*/0,
                       /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 985 —— 吸取成功则下{0}回合对手属性技能无效 ──
EffectResult effect_985_drain_boosts_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 985, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 985, args,
                     "需要 [owner, target, rounds]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner
                         && rounds > 0, 985, args, "需要 owner/target ∈ {0,1} 且 rounds > 0");
    if (!g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败（无提升可吸 / 被免消除强化挡下）→ 不封属
    }
    // "下{0}回合"：NextRounds 起点恒 R+1（§五.0 不叠顺延）→ 窗口 1 回合、R+1 ROUND_START
    // 授 duration={0} 票（避免当回合直授被回合末 tick 提前吃掉一格）。
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 985,
                                /*window_rounds=*/1, {owner, opp, rounds},
                                &effect_985_seal_grant_tick);
    return EffectResult::kOk;
}

// ── 1062 —— 吸取成功则吸取对手最大体力的1/{0} ──
EffectResult effect_1062_drain_boosts_drain_hp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1062, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1062, args,
                     "需要 [owner, target, denom]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner
                         && denom > 0, 1062, args, "需要 owner/target ∈ {0,1} 且 denom > 0");
    if (!g_core || !g_core->transfer_stat_boosts || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败 → 不吸体力
    }
    // max_hp/{denom} 的"吸取"语义整体在内核（FIXED 固定伤害 + heal_impl 回等量）。
    (void)g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, denom);
    return EffectResult::kOk;
}

// ── 1214 —— 对手处于异常状态则吸取对手{0}点体力 ──
EffectResult effect_1214_anomaly_gate_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1214, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1214, args,
                     "需要 [owner, target, amount]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner
                         && amount > 0, 1214, args, "需要 owner/target ∈ {0,1} 且 amount > 0");
    if (!g_core || !g_core->has_active_anomaly || !g_core->deal_pink_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    bool foe_abnormal = false;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, opp, id)) {
            foe_abnormal = true;
            break;
        }
    }
    if (!foe_abnormal) {
        return EffectResult::kOk;   // 门槛：对手不处于任何异常 → 不吸
    }
    // 吸 {0} 点：粉伤·固定档打对手 + 等量回自身（840 先例同款；字面口径记档）。
    g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(985, effect_985_drain_boosts_seal_attribute)
SKILL_EFFECT(1062, effect_1062_drain_boosts_drain_hp)
SKILL_EFFECT(1214, effect_1214_anomaly_gate_drain)
