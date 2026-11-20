// 精灵专属粉转真协调：库坦 1737 / 启灵元神 1531 / 圣谕邪魇 2077（场景 052）。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// 技能效果注册函数 - 由动态库加载器调用

// ══════════════════════════════════════════════════════════════════════════
// 粉转真一族（用户 2026-09-15 指定的三条，正好覆盖三种判据）
//
// ★ 共同点：粉伤**即时结算**——`deal_pink_damage` / `fixed_damage` 返回时
//   `ctx->resolvedPink` 就是本段的结果，**效果当场读它判定**即可。
//   不需要注册任何常驻条目，也就不可能把"我这次要转真"串给下一个无关的粉伤效果。
//
//   判据速查（`PinkDamageResolved`）：
//     `final > 0`                    → 真的受到粉伤了
//     `final <= 0 && absorbed == 0`  → 被免疫/抗性/免减挡下（**免疫型**）
//     `absorbed > 0`                 → 护罩参与了；`final <= 0` 即"护罩吃光 → 没受到"
//     `raw`                          → 本段原始量（转真按它转）
// ══════════════════════════════════════════════════════════════════════════

// ── 库坦「残戮砍杀」36575 / effect 1737 ──
// 原文：「附加自身已损失PP值总和x{0}的固定伤害，**对手免疫固定伤害时额外附加等量的真实伤害**」
//   Args = [owner, target, {0}=系数]。
// ★ **免疫型**粉转真：判据是 `final <= 0 && absorbed == 0`（被免粉/抗性/免减削到 0）。
//   护罩吃光**不算**——那是"吸收"，不是"免疫"（`absorbed > 0` 即排除）。
EffectResult effect_1737_pp_fixed_with_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int coef = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || coef <= 0) {
        return EffectResult::kOk;
    }
    // 自身已损失 PP 总和（`pp < 0` = 无限 PP，跳过）
    const ElfPet& self = ctx->getPet(owner);
    int lost_pp = 0;
    for (const Skills& sk : self.skills) {
        if (sk.pp < 0 || sk.maxPP < 0) {
            continue;   // pp == -1 = 无限 PP
        }
        lost_pp += std::max(0, sk.maxPP - sk.pp);
    }
    if (lost_pp <= 0) {
        return EffectResult::kOk;   // 满 PP → 附加 0 点，不必发这一段
    }
    const int amount = lost_pp * coef;
    if (g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    // ★ 转真判定：读**刚才那一段**的即时结算结果
    const PinkDamageResolved& r = ctx->resolvedPink;
    if (r.target == target && r.final <= 0 && r.absorbed == 0
        && g_core && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, r.raw, owner);   // 等量真伤
    }
    return EffectResult::kOk;
}

// ── 启灵元神「神之束缚」27020 / effect 1531 ──
// 原文：「{0}回合内每回合使用技能附加{1}{2}」＋技能描述「…开始慢慢附加固定伤害了，
//        **若对手免疫则会额外承受真实伤害的神罚**」
//   Args = [owner, target, {0}=回合数, {1}=数值, {2}=伤害档（0=固定伤害）]。
// ★ 逐回合触发：注册一条**回合类效果**到"每回合使用技能"时点（先/后出手各一格，
//   同一位玩家当回合只会在其中一格出手 → 只触发一次，见 FSM 的 first/second_mover 分派）。
// ★ 粉转真判据同 1737（**免疫型**）。
constexpr int kShenshenFixedTicks = 1531;

EffectResult effect_1531_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    const PinkDamageResolved& r = ctx->resolvedPink;
    if (r.target == target && r.final <= 0 && r.absorbed == 0
        && g_core && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, r.raw, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_1531_round_fixed_with_true(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    // 先出手/后出手各挂一格：同回合只有一格会以 owner 为 mover 执行 → 只附加一次。
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, kShenshenFixedTicks,
                          rounds, {owner, target, amount}, &effect_1531_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, kShenshenFixedTicks,
                          rounds, {owner, target, amount}, &effect_1531_tick);
    return EffectResult::kOk;
}

// ── 圣谕邪魇「神明送葬」28442 / effect 2077 ──
// 原文：「{0}回合内使用技能吸取对手{1}点体力，自身体力低于1/{2}时效果翻倍，
//        **吸取后若对手体力减少值低于180则附加{3}点真实伤害**与1层自身所处形态相应的圣诰/邪诲」
//   Args = [owner, target, {0}=回合数, {1}=吸取量, {2}=体力分档分母, {3}=真伤量]。
//   ★ 阈值 **180** 是模板里写死的字面值（effect_info 原文如此），不是参数。
// ★ **高额减粉型**粉转真：判据不是"被免疫"，而是"**实际减少值**没到阈值"——
//   减粉/抗性/护罩把 280 削到 180 以下就补真伤。这正是"专门针对高额减粉"的那一支。
// ⚠️ 用 `final`（**实际结算到本体的量**）而不是 `raw`：护罩吃光 → final=0 < 180 → 也补。
constexpr int kShenmingDrainTicks = 2077;
constexpr int kShenmingLowThreshold = 180;   // 模板字面值（"低于180"）

EffectResult effect_2077_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    int drain = args.int_args[2];
    const int denom = args.int_args[3];
    const int true_amount = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || drain <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
    if (denom > 0 && max_hp > 0 && self.hp * denom < max_hp) {
        drain *= 2;   // 自身体力低于 1/{2} → 效果翻倍
    }
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::FIXED, owner);
    // 吸取：**按实际造成的伤害**回血（`final` = 结算到本体的量）
    const PinkDamageResolved& r = ctx->resolvedPink;
    const int dealt = (r.target == target) ? std::max(0, r.raw - r.final - r.absorbed) : 0;
    if (dealt > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, dealt);
    }
    // ★ 转真判定：**实际结算到本体的量**低于 180 → 补 {3} 点真伤
    const int settled = (r.target == target) ? std::max(0, r.final) : 0;
    if (settled < kShenmingLowThreshold && true_amount > 0 && g_core->deal_true_damage) {
        g_core->deal_true_damage(ctx, target, true_amount, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_2077_round_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int drain = args.int_args[3];
    const int denom = args.int_args[4];
    const int true_amount = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0 || drain <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick_args{owner, target, drain, denom, true_amount};
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, kShenmingDrainTicks,
                          rounds, tick_args, &effect_2077_drain);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, kShenmingDrainTicks,
                          rounds, tick_args, &effect_2077_drain);
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // ── 粉转真一族（三种判据各一条，用户 2026-09-15 指定）──
    // 1737 免疫型 / 1531 免疫型（逐回合附加）/ 2077 高额减粉型（实际减少值 < 180）。
    SKILL_EFFECT(1737, effect_1737_pp_fixed_with_true);   // 残戮砍杀（库坦）
    SKILL_EFFECT(1531, effect_1531_round_fixed_with_true); // 神之束缚（启灵元神）
    SKILL_EFFECT(2077, effect_2077_round_drain);           // 神明送葬（圣谕邪魇）

}  // namespace
