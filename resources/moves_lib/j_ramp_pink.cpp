#include <utils/sbs_rand.h>
// J 组 J138 · 累积/条件附粉族 —— 1366×12 / 903×12 / 1132×12（2026-10-04，
// worktree ../agents/j138）
//
// 家族语义（effect_info 模板即权威）：
//   1366（args_num=3）"附加攻击伤害{0}%的百分比伤害，每次使用增加{1}%，最高{2}%"
//   903 （args_num=1）"自身体力高于对手时附加自身最大体力值{0}%的百分比伤害"
//     —— 笔走龙蛇 30005 [30]（纯链物）、霹雳凝止 36540 [30]（纯链特）。
//   1132（args_num=4）"随机恢复自身{0}-{1}点体力，自身不处于能力提升状态时变为{2}-{3}点体力"
//     —— 福光普照 30485 [200,400,300,500]（纯链特）等×4。
//
// 实现口径：
//   - 1366 = DETECT 攻方桶读 final → deal_pink(PERCENT_VALUE, final×pct%)（869 同族；
//     ⚠️ 869 的 4399 口径是"固定伤害"（FIXED），本条模板/语料均为"百分比伤害"字面 →
//     PERCENT_VALUE 红伤比例粉，两兄弟口径分叉记档）。**不逐次注册**（damage pipeline
//     条目会累积堆叠）——首次施放注册一条常驻 DETECT（owner 在场有效），之后每次施放
//     只 bump plugin_storage 审计槽（F9 782 首创）：pct += {1} 钳 {2}；回调只读槽施加。
//     首用施加 {0}%、第二用 {0}+{1}%…与"每次使用增加"序一致。
//   - 903 = SKILL_EFFECT hp 对比门（我 > 对手在场宠）→ deal_pink(PERCENT_VALUE,
//     max_hp_of(self)×{0}%)（J7 852 直系零回合版；门不成立不产粉）。
//   - 1132 = roll band heal：heal_amount(lo + rng.next()%(hi-lo+1))；count_boost_kinds==0
//     （六槽无正槽）→ 第二段 band（J14 850 掷点口径；band 闭区间）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   1366 [pct0,step,cap]：链上 12/12 自洽（纯链 0，场景载体选链上兄弟良性者）
//   903  [pct]：30005/36540 纯链×2…
//   1132 [lo1,hi1,lo2,hi2]：30485/34609/35960/36088 纯链×4…
//
// 动工前置三查（2026-10-04）：①内核 grep 1366/903/1132 无硬编码 ②effect_meta 无分类
// 牵连 ③moves_lib 无既有注册——真缺口（deal_pink_damage/DETECT/audit slot 既有设施；
// 869 为在途兄弟单 J131，本条独立实现不共文件）。
// 场景回归：test/scenario/scenario_2330_ramp_pink.cpp（号段 2330-2339）。

#include "common.h"

namespace {

constexpr int k1366PctBase = 990136600;   // 审计槽（+owner）：当前 pct

// ── 1366 —— 附加攻击伤害{0}%粉，每次使用增加{1}%，最高{2}% ──
// int_args = [owner, target, pct0, step, cap]（args_num=3）。
EffectResult effect_1366_ramp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1366, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1366, args,
                     "1366 布局=[owner,target,pct0,step,cap]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct0 = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1366, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct0 > 0 && step > 0 && cap >= pct0, 1366, args,
                     "需要 pct0>0 step>0 cap>=pct0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int key = k1366PctBase + owner;
    if (ctx->plugin_storage.find(key) == ctx->plugin_storage.end()) {
        ctx->plugin_storage[key] = pct0;   // 首用：落初值 + 注册常驻 DETECT
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
                const int pct = any_int_or(c->plugin_storage[k1366PctBase + owner], 0);
                if (pct <= 0) {
                    return;
                }
                (void)g_core->deal_pink_damage(c, target, d.final * pct / 100,
                                               DamageKind::PERCENT_VALUE, owner);
            });
    } else {
        const int cur = any_int_or(ctx->plugin_storage[key], pct0);
        ctx->plugin_storage[key] = std::min(cur + step, cap);   // 每次使用递增、钳上限
    }
    return EffectResult::kOk;
}

// ── 903 —— 自身体力高于对手时附加自身最大体力值{0}%的百分比伤害 ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_903_highhp_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 903, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 903, args,
                     "903 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     903, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 903, args, "需要 pct > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hp <= ctx->getPet(opp).hp) {
        return EffectResult::kOk;   // hp 对比门（严格高于）
    }
    const int pink = max_hp_of(ctx, owner) * pct / 100;
    if (pink > 0) {
        (void)g_core->deal_pink_damage(ctx, opp, pink, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 1132 —— 随机恢复自身{0}-{1}点体力，无提升时变为{2}-{3}点体力 ──
// int_args = [owner, target, lo1, hi1, lo2, hi2]（args_num=4）。
EffectResult effect_1132_random_band_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1132, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1132, args,
                     "1132 布局=[owner,target,lo1,hi1,lo2,hi2]");
    const int owner = args.int_args[0];
    const int lo1 = args.int_args[2];
    const int hi1 = args.int_args[3];
    const int lo2 = args.int_args[4];
    const int hi2 = args.int_args[5];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1132, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(lo1 > 0 && hi1 >= lo1 && lo2 > 0 && hi2 >= lo2, 1132, args,
                     "需要 band 端点合法");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    bool boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            boosted = true;
            break;
        }
    }
    const int lo = boosted ? lo1 : lo2;
    const int hi = boosted ? hi1 : hi2;
    const int amount = choose_skill_range(*ctx, 1132, 0, owner, owner, lo, hi);
    (void)g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1366, effect_1366_ramp_pink)
SKILL_EFFECT(903, effect_903_highhp_pink)
SKILL_EFFECT(1132, effect_1132_random_band_heal)
