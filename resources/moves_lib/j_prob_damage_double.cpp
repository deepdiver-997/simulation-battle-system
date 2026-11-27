#include <utils/sbs_rand.h>
// J 组 J196 · 概率伤害翻倍族 —— 1 宏/10 载体（2026-10-22，worktree ../agents/j196）
//
// 家族语义（effect_info 模板即权威）：
//   1481 [1] {0}%的概率造成伤害翻倍，对手处于能力下降状态时概率翻倍
//
// 实现口径：
//   - DETECT 攻方桶（register_damage_effect，541/1104 同通道）读管线后 resolvedDamage：
//     自卡 attackerId==owner / defenderId==target / isRed / final>0。
//   - 概率 p = {0}%；对手 6 槽（含命中）任一负等级 → p×2，封顶 100（"概率翻倍"）。
//     p>=100 短路不掷骰；否则 rng.next()%100 < p。
//   - 命中 → final = checked_pct_mul(final, 100)（×2；计算乘区统一入口，溢出响亮报错）。
//   - 审计槽 plugin_storage[k1481AuditBase+owner] = rolled*1000 + p（场景可精确断言
//     "本次申报概率与是否命中"，不依赖随机结果猜分支）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1481 [概率]：33423 未来·音速火拳 [50]、34929 聚邪天雷引 [50]、34012 黯天斩 [50]、
//        38508 苦痛奇迹 [30]…（10/10 自洽，9 个 50 + 1 个 30）
//
// 动工前置三查（2026-10-22）：①内核 grep 1481 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册——真缺口。
// 场景回归：test/scenario/scenario_5180_prob_damage_double.cpp（号段 5180-5189）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <numerical-calculation/calculation.h>

namespace {

constexpr int k1481AuditBase = 990148100;   // +owner：rolled*1000 + 本次申报概率

// 对手处于能力下降状态 = 6 槽（含命中）任一负等级（962/J182 判据）。
bool j196_foe_has_drop(const BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1481 —— {0}%的概率造成伤害翻倍，对手处于能力下降状态时概率翻倍 ──
// int_args = [owner, target, 概率]（args_num=1）。
EffectResult effect_1481_prob_damage_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1481, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1481, args,
                     "1481 布局=[owner,target,概率]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1481, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0 && pct <= 100, 1481, args, "需要 概率∈(0,100]");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤判定
            }
            int p = pct;
            if (j196_foe_has_drop(c, target)) {
                p = std::min(100, p * 2);   // 对手降态：概率翻倍（封顶 100）
            }
            const bool rolled = (p >= 100) || ((c->rng().next() % 100) < p);
            c->plugin_storage[k1481AuditBase + owner] = (rolled ? 1000 : 0) + p;
            if (rolled) {
                c->resolvedDamage.final =
                    checked_pct_mul(c->resolvedDamage.final, 100, "effect_1481");
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1481, effect_1481_prob_damage_double)
