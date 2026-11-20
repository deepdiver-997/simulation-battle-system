// J 组 J200 · 提升态攻击回血族 —— 1 宏/20 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   489 [1] 若自身处于能力提升状态，则每次攻击恢复自身体力的1/{0}
//
// 实现口径：
//   - DETECT 攻方桶（541/1104 同通道）：一次攻击落地前读 resolvedDamage，只认
//     attacker==self / defender==opp / isRed 的本次攻击（"每次攻击"= 逐刀，不按技能次数）。
//   - 门 = 自身六槽（含命中；等级槽 0..5）任一正等级（962/1472 同判据）。
//   - 回血 = heal_amount(self, max_hp/{0})（heal_impl 通道：封回血/恢复修正生效；
//     满血被 clamp 属正常，场景用半血观测）。
//   - 审计槽 plugin_storage[k489AuditBase+self] = 本刀触发次数（1=触发；无=键缺席）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   489 [回血分母]：33921 星粒碰撞 [4]（纯链，物 140，场景载体）、34147 狂翼疾风 [3]、
//        14295 龙王啸天破 [4]、15598 千叶醉天舞 [4]…（20 载体，分母 3~5 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 489 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(489)——真缺口。
// 场景回归：test/scenario/scenario_5250_boost_attack_heal.cpp（号段 5250-5259）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k489AuditBase = 990489100;   // +self：本刀触发次数

bool j200_has_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 489 —— 若自身处于能力提升状态，则每次攻击恢复自身体力的1/{0} ──
// int_args = [owner, target, 回血分母]（args_num=1）。
EffectResult effect_489_boost_attack_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 489, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 489, args,
                     "489 布局=[owner,target,回血分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     489, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0, 489, args, "需要 分母 > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, self, DamageEffectCategory::DETECT,
        [self, opp, denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != self) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != self || d.defenderId != opp || !d.isRed) {
                return;   // 只认本次自身打出的红伤（属性技/对手伤害不计）
            }
            if (!j200_has_boost(c, self)) {
                return;   // 门：自身处于能力提升状态
            }
            if (!g_core || !g_core->heal_amount) {
                return;
            }
            const int before = c->getPet(self).hp;
            const int heal = max_hp_of(c, self) / denom;
            if (heal <= 0) {
                return;
            }
            (void)g_core->heal_amount(c, self, heal);
            if (c->getPet(self).hp > before) {
                const auto it = c->plugin_storage.find(k489AuditBase + self);
                const int n = (it == c->plugin_storage.end()) ? 0 : any_int_or(it->second);
                c->plugin_storage[k489AuditBase + self] = n + 1;
            }
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(489, effect_489_boost_attack_heal)
