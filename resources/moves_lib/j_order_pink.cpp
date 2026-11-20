// J 组 J150 · 先后手附粉族 —— 932×10 / 815×10 / 853×11（2026-10-04，
// worktree ../agents/j150）
//
// 家族语义（effect_info 模板即权威）：
//   932（args_num=1）"先出手时附加自身攻击{0}%的百分比伤害"
//     —— 焚天之刺 30082 [50]（纯链物）、天翼翔斩 30209 [30]（纯链物）。
//   815 （args_num=1）"后出手时附加自身当前体力{0}%的百分比伤害"
//     —— 疯决乱拳冲 19617 [30]（纯链物）、幻尘突进 33586 [30]、夜影利刃 35234 [30]。
//   853 （args_num=3）"附加自身最大体力值与速度值总和{0}%的百分比伤害，每次使用增加{1}%，最高{2}%"
//     —— 链上 11 载体自洽（纯链 0，场景选兄弟良性链）。
//
// 实现口径：
//   - 932/815 = SKILL_EFFECT acting_side_is_first 门（680/J17/J114 域内判据，先短路
//     后掷骰不适用——无概率）→ deal_pink(PERCENT_VALUE)：
//     932 基础=攻击面板 getTempAbilityValue(PHYSICAL_ATTACK)×{0}%；
//     815 基础=当前体力 getPet(owner).hp×{0}%。
//   - 853 = J138 1366 直系（常驻 DETECT 一条 + plugin_storage 审计槽按次 bump 防堆叠），
//     基础=max_hp_of + getTempAbilityValue(SPEED)。
//
// 动工前置三查（2026-10-04）：①内核 grep 932/815/853 无硬编码 ②effect_meta 无分类
// 牵连 ③moves_lib 无既有注册——真缺口。
// 场景回归：test/scenario/scenario_2450_order_pink.cpp（号段 2450-2459）。

#include "common.h"

namespace {

constexpr int k853PctBase = 990085300;   // 审计槽（+owner）：当前 pct

// ── 932 —— 先出手时附加自身攻击{0}%的百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_932_first_atk_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 932, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 932, args,
                     "932 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     932, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 932, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 先手门
    }
    const int pink = ctx->ws.getTempAbilityValue(owner, NumericalPropertyIndex::PHYSICAL_ATTACK) *
                     pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 815 —— 后出手时附加自身当前体力{0}%的百分比伤害 ──
EffectResult effect_815_second_hp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 815, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 815, args,
                     "815 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     815, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 815, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手门
    }
    const int pink = ctx->getPet(owner).hp * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 853 的 DETECT 回调：基础=(max_hp+speed)，pct 读审计槽。──
void j150_register_853_detect(BattleContext* ctx, int owner, int target) {
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            const int pct = any_int_or(c->plugin_storage[k853PctBase + owner], 0);
            if (pct <= 0) {
                return;
            }
            const int base = max_hp_of(c, owner) +
                             c->ws.getTempAbilityValue(owner, NumericalPropertyIndex::SPEED);
            const int pink = base * pct / 100;
            if (pink > 0) {
                (void)g_core->deal_pink_damage(c, target, pink, DamageKind::PERCENT_VALUE, owner);
            }
        });
}

// ── 853 —— 附加(最大体力+速度)和{0}%粉，每次使用增加{1}%，最高{2}% ──
// int_args = [owner, target, pct0, step, cap]（args_num=3）。
EffectResult effect_853_ramp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 853, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 853, args,
                     "853 布局=[owner,target,pct0,step,cap]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct0 = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     853, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct0 > 0 && step > 0 && cap >= pct0, 853, args,
                     "需要 pct0>0 step>0 cap>=pct0");
    const int key = k853PctBase + owner;
    if (ctx->plugin_storage.find(key) == ctx->plugin_storage.end()) {
        ctx->plugin_storage[key] = pct0;
        j150_register_853_detect(ctx, owner, target);
    } else {
        const int cur = any_int_or(ctx->plugin_storage[key], pct0);
        ctx->plugin_storage[key] = std::min(cur + step, cap);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(932, effect_932_first_atk_pink)
SKILL_EFFECT(815, effect_815_second_hp_pink)
SKILL_EFFECT(853, effect_853_ramp_pink)
