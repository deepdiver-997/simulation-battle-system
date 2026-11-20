// J54 · 吸取条件族二 —— 628×19 / 963×18 / 977×17（2026-10-02，worktree ../agents/j54）
//
// 家族语义（effect_info 官方模板 + 载体切片核对），三条全是"条件/门槛 → 吸取/回血"：
//
// 628（arg_count=1）"若对手处于能力下降状态则造成伤害的{0}%恢复体力"：
//   - 条件 = 对手任一负槽（六槽含命中，J14 has_any_stat_drop 同判据）；
//   - 回血 = 造成伤害 ×{0}% → heal_amount——读 **裸伤台账** ws.raw_attack_damage
//     （422/101/1249 同款口径："挡伤/锁伤改变不了它"；bailuo 2440 同构）；
//   - ⚠️ 注册时点：本条读"本次攻击的伤害"，SKILL_EFFECT 桶跑在伤害管线**之前**
//     （台账还是 0）→ 内核 effect_register_state 加 case 628 归位
//     BATTLE_FIRST_AFTER_ACTION（101/422 同款一行，src/entities/skills.cpp）。
//   - 实例：弑灭屠神决 17044 [628]|[40]、折翼之爪 35261 [628]|[100]。
// 963（arg_count=1）"若自身处于能力提升状态则吸取对手最大体力的1/{0}"：
//   - 条件 = 1257 取反族：自身任一**正槽**（ability_levels[side][i] > 0）；
//   - 吸取 = g_core->drain_hp（打 1/{0} 对手最大体力、按实际落地回吸），
//     J16 1257 直系（同走默认 SKILL_EFFECT 时点——suffix 吸取不读伤害台账）。
//   - 实例：地龙啸天破 30287 [963]|[3]、圣枪疾刺 30441 [963]|[4]。
// 977（arg_count=2）"吸取对手最大体力的1/{0}，自身体力低于1/{1}时吸取效果翻倍"：
//   - 无条件先吸 1/{0}；低体判据 = 吸取**前** hp < max/{1}（J16 1290 恢复前判口径
//     ——吸取后判会被吸取回血本身翻案）；翻倍 = 同量再来一次（再 drain 1/{0}，
//     J12 964"翻倍"同口径：加一次同量，不取 1/{0/2} 的慷慨读法）。
//   - 实例：邪魔破世眼 19728 [976,977]|[1,4,2]（976 消回合类空场=良性）。
//
// 动工前置三查（2026-10-02）：①内核硬编码——628/963/977 零命中（无 effect_register_state
//   归位行、无解析模板兜底、无 RuleCenter 票）；②effect_meta 无分类牵连；③moves_lib
//   无既有注册——真缺口（grep 仅别的效果链注释里出现载体号）。载体切片：side_effect
//   表 arg_count 628=1/963=1/977=2 与 effect_info.args_num 一致；54 载体链切片自洽，
//   仅记档 32102（628 链尾参差）/29414（977 链尾参差）单载体不阻塞。
// 先例：1257/1290（j_drain_cond.cpp，J16）、2440（bailuo_skills.cpp 裸伤×%回血）、
//   has_any_stat_drop（j_prob_fallback3.cpp，J14；单 dylib 匿名空间不跨 TU → 就地重写）、
//   drain_hp（CoreApi 163）、heal_amount（common.h 口径）。
// 场景回归：test/scenario/scenario_1400_drain_cond2.cpp（1400 号段，一文件三宏）。

#include "common.h"

namespace {

// ── 判据助手（J14 has_any_stat_drop 就地重写：单 dylib 匿名空间符号不跨 TU）──
bool j54_has_any_stat_drop(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

bool j54_has_any_stat_up(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 628 —— 若对手处于能力下降状态则造成伤害的{0}%恢复体力 ──
// 注册时点 = AFTER_ACTION（内核 case 628 归位行，见文件头）；台账=本次攻击裸伤。
EffectResult effect_628_cond_lifesteal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 628, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 628, args,
                     "布局=[owner,target,pct]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 628, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(pct > 0, 628, args, "需要 pct>0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (!j54_has_any_stat_drop(ctx, target)) {
        return EffectResult::kOk;   // 前提：对手处于能力下降状态
    }
    const int dealt = ctx->ws.raw_attack_damage[self];
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, self, dealt * pct / 100);
    }
    return EffectResult::kOk;
}

// ── 963 —— 若自身处于能力提升状态则吸取对手最大体力的1/{0} ──
EffectResult effect_963_cond_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 963, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 963, args,
                     "布局=[owner,target,denom]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 963, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(denom > 0, 963, args, "需要 denom>0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    if (!j54_has_any_stat_up(ctx, self)) {
        return EffectResult::kOk;   // 前提：自身处于能力提升状态
    }
    (void)g_core->drain_hp(ctx, self, target, denom);
    return EffectResult::kOk;
}

// ── 977 —— 吸取对手最大体力的1/{0}，自身体力低于1/{1}时吸取效果翻倍 ──
EffectResult effect_977_lowhp_double_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 977, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 977, args,
                     "布局=[owner,target,denom,thresh_denom]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int thresh_denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 977, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(denom > 0 && thresh_denom > 0, 977, args, "需要 两分母 > 0");
    if (!g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    // 低体判据用吸取前的 hp（吸取后判会被回血本身翻案，1290 同口径）。
    const bool low_hp = ctx->getPet(self).hp < max_hp_of(ctx, self) / thresh_denom;
    (void)g_core->drain_hp(ctx, self, target, denom);
    if (low_hp) {
        // 翻倍 = 同量再来一次（964 口径）；首吸击杀目标时由 drain 的
        // TARGET_DEFEATED 挡下（第二吸自然落空）。
        (void)g_core->drain_hp(ctx, self, target, denom);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(628, effect_628_cond_lifesteal)
SKILL_EFFECT(963, effect_963_cond_drain)
SKILL_EFFECT(977, effect_977_lowhp_double_drain)
