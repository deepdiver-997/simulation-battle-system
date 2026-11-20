// J 组 J204 · 低伤自身升级族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   107 [2] 若本次攻击造成的伤害小于{0}，则自身{1}提升1个等级
//
// 实现口径：
//   - DETECT 攻方桶（541/1104 同通道）读本次攻击 final；只认自身→对手红伤，
//     且只在**施放当回合的首刀**有效（armed/round 双闸，miss/打盔不残留跨回合误触发）。
//   - final < {0}（严格小于）→ stat_change(self, 能力槽 {1}, +1)（自增益真原语，
//     本体/视图同步，±6 钳制由原语管）。
//   - 审计槽 plugin_storage[k107AuditBase+self] = 本刀实际伤害（场景精确断门）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   107 [伤害阈值,能力槽]：12112 双重暗影 [250,0]（物 150，攻槽）、
//        11841 紫雾霜冻 [100,2]（特 150，防槽）、12742 疾风冲刺 [150,4]（速槽）…
//        （9/9 自洽；{1} 值 0/2/4 = 引擎能力槽语义序，与文案"攻击/防御/速度"对应）
//
// 动工前置三查（2026-10-22）：①内核 grep 107 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(107)——真缺口。
// 场景回归：test/scenario/scenario_5290_low_damage_level_up.cpp（号段 5290-5299）。

#include "common.h"

#include <primitives/battle_primitives.h>

#include <memory>

namespace {

constexpr int k107AuditBase = 990107100;   // +self：本刀实际伤害

// ── 107 —— 若本次攻击造成的伤害小于{0}，则自身{1}提升1个等级 ──
// int_args = [owner, target, 伤害阈值, 能力槽]（args_num=2）。
EffectResult effect_107_low_damage_level_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 107, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 107, args,
                     "107 布局=[owner,target,伤害阈值,能力槽]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int slot = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     107, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0, 107, args, "需要 伤害阈值 > 0");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 107, args,
                     "需要 能力槽 ∈ [0,6)");
    auto armed = std::make_shared<bool>(true);
    auto registered_round = std::make_shared<int>(ctx->roundCount);
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, opp, threshold, slot, armed, registered_round](BattleContext* c,
                                                              int bucket_owner) {
            if (!c || bucket_owner != self || !*armed) {
                return;
            }
            if (c->roundCount != *registered_round) {
                return;   // 首刀未结算 → 不留跨回合误触发
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId != opp || !d.isRed) {
                return;
            }
            *armed = false;
            c->plugin_storage[k107AuditBase + self] = d.final;
            if (d.final < threshold && g_core && g_core->stat_change) {
                (void)g_core->stat_change(c, self, slot, /*delta=*/1);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(107, effect_107_low_damage_level_up)
