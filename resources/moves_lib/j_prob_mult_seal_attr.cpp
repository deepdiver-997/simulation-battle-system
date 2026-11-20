// J 组 J244 · 概率倍伤/未触发封属族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   921（args_num=3）"{0}%概率造成的攻击伤害为{1}倍，未触发则{2}回合内令对手使用的属性技能无效"
//
// 实现口径：
//   - SKILL_EFFECT 掷 {0}%（roll_percent；0/100 显式档在位即消费，红线7）。
//   - **触发**（攻击伤害 ×{1}）：DETECT 攻方桶改写本次 final
//     `checked_pct_mul(final, ({1}-1)*100)`（1481 概率倍伤直系通道；armed + registered_round
//     一次性闸，miss/跨回合不残留误触发）。
//   - **未触发**（封属窗 {2} 回合）：seal_skill(attribute=true, attack=false,
//     count=0 时长型, duration={2}, penetrable=true, ON_STAGE)（289/1922 同款封属通道）。
//   - 审计槽 plugin_storage[990921100+owner] = rolled（0/1；场景逐局判分支）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   921 [概率,倍数,回合]：36906 猫叒斩 [30,3,2]（纯链，物 140）、19526 何逢万古天
//     [180,921]|[10,3,2]、31380 凌冰穿云箭 [921,1006]|[15,3,2]、31661 逝者冥音
//     [928,921]|[20,2,2]、32297 愤怒之碎 [25,2,2]、32984 天石破魂击 [20,2,2]、
//     33205 断邪疾刃 [25,2,2]、35967 涟漪轻泛 [50,2,2]、37023 创灵极星阵 [30,3,2]
//     （9/9 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 921 无硬编码 ②effect_info 模板唯一、
// custom_effect_overrides/programs 无行 ③registry.json moves 集不含 921，全库
// moves_lib/soul_lib 并集（含各 worktree）无 SKILL_EFFECT(921)——真缺口。
// 场景回归：test/scenario/scenario_5750_prob_mult_seal_attr.cpp（号段 5750-5759）。

#include "common.h"

#include <numerical-calculation/calculation.h>

#include <memory>

namespace {

constexpr int k921AuditBase = 990921100;   // +owner：rolled（0/1）

// ── 921 —— {0}%概率倍伤，未触发则{2}回合封属 ──
// int_args = [owner, target, 概率, 倍数, 回合]（args_num=3）。
EffectResult effect_921_prob_mult_or_seal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 921, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 921, args,
                     "921 布局=[owner,target,概率,倍数,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int mult = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     921, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 921, args, "需要 概率 ∈ [0,100]");
    EFFECT_ARG_GUARD(mult >= 1, 921, args, "需要 倍数 ≥ 1");
    EFFECT_ARG_GUARD(rounds >= 0, 921, args, "需要 回合 ≥ 0");

    const bool rolled = roll_percent(pct);
    ctx->plugin_storage[k921AuditBase + owner] = rolled ? 1 : 0;
    if (!rolled) {
        // 未触发 → {2} 回合封属性技（时长型，count=0）。
        if (rounds > 0 && g_core && g_core->seal_skill) {
            (void)g_core->seal_skill(ctx, owner, target, /*effect_id=*/921,
                                     /*attribute=*/true, /*attack=*/false,
                                     /*count=*/0, /*duration_rounds=*/rounds,
                                     /*penetrable=*/true, /*source_slot=*/-1,
                                     EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                     /*chance_pct=*/100,
                                     /*consumed_when_pierced=*/false);
        }
        return EffectResult::kOk;
    }
    if (mult <= 1) {
        return EffectResult::kOk;   // ×1 = 无改写
    }
    auto armed = std::make_shared<bool>(true);
    auto registered_round = std::make_shared<int>(ctx->roundCount);
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, mult, armed, registered_round](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !*armed) {
                return;
            }
            if (c->roundCount != *registered_round) {
                return;   // 当次未结算 → 不跨回合误触发
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;
            }
            *armed = false;
            c->resolvedDamage.final = checked_pct_mul(
                c->resolvedDamage.final, (mult - 1) * 100, "effect_921 概率倍伤");
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(921, effect_921_prob_mult_or_seal)
