// J 组 J125 · 回复族二 —— 407×12 / 404×13 / 541×12（2026-10-04，worktree ../agents/j125）
//
// 家族语义（effect_info 模板即权威）：
//   407（args_num=3）"下回合起，每回合{0}等级+{1}，持续{2}回合"
//     —— 神怒焰焚天 15780 [2,1,3]，4399"下3回合每回合特攻+1"→ {0}=**官方码序**
//        （0攻1防2特攻3特防4速；J121 473 同坑同映射）→ 引擎槽 {0→0,1→2,2→1,3→3,4→4}。
//   404（0 参）"恢复双方全部体力"
//   541（args_num=2）"造成的攻击伤害若低于{0}则恢复自身{1}点体力"
//     —— 炼狱冥火诀 15159 [300,200]（纯链特150）。
//
// 实现口径：
//   - 407 = NextRounds({2}) ROUND_START tick（J119 1635 NextRounds 同族/J6 每回合族）：
//     窗 [R+1, R+{2}] 恰好"下回合起持续{2}回合"，每回合 stat_change(自身, 槽, +{1})
//     （正增益不吃免弱，真原语本体/视图同步）。
//   - 404 = heal_max_hp_fraction(1) 双侧各一次（heal 原语吃封回血/恢复修正；无判定门，
//     满血侧自然钳 0）。
//   - 541 = DETECT 攻方桶（989 直系同形：register_damage_effect(DETECT, owner, DETECT)，
//     回调读 resolvedDamage：attackerId/defenderId/isRed/final>0 自卡）——final **严格 <**
//     {0} → heal_amount(自身, {1})（704『低于』同款整数直乘口径的严格读法）。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   407  [槽,增量,回合]：15780 神怒焰焚天 [2,1,3]（纯链特160）、21716 战争旋律
//        [0,1,2]、23446 高速咏唱 [4,1,2]…（12/12 自洽）
//   404  0 参：28466 一番茄调味（纯链属）、21699 地狱变相 [55,404]…（13/13 自洽）
//   541  [阈值,回复]：15159 炼狱冥火诀 [300,200]（纯链）、32744 深渊震荡 [300,300]、
//        34486 极速冰弹 [250,250]…（12/12 自洽）
//
// 动工前置三查（2026-10-04）：①内核 grep 407/404/541 无硬编码 ②effect_meta：404 命中
// "恢复"→Heal 分类（仅文档性，不拦截派发）、407/541 Other ③moves_lib 无既有注册——
// 真缺口（register_damage_effect DETECT/heal_amount 均既有设施）。
// 场景回归：test/scenario/scenario_2190_cond_heal2.cpp（号段 2190-2199）。

#include "common.h"

namespace {

// 官方码序（0攻1防2特攻3特防4速）→ 引擎槽（0攻1特攻2防3特防4速5命中）。
// 神怒焰焚天 [2,1,3]="特攻+1" 双源钉死；H2 158/J121 473 同映射。
int j125_official_slot_to_engine(int official) {
    switch (official) {
        case 0: return 0;
        case 1: return 2;
        case 2: return 1;
        case 3: return 3;
        case 4: return 4;
        case 5: return 5;
        default: return -1;
    }
}

// ── 407 的 ROUND_START tick —— payload={owner, 引擎槽, 增量}。
EffectResult j125_round_stat_up_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int slot = args.int_args[1];
    const int delta = args.int_args[2];
    if (owner < 0 || owner > 1 || slot < 0 || slot >= BattleContext::kAbilityLevelSlotCount
        || delta == 0) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(ctx, owner, slot, delta);
    return EffectResult::kOk;
}

// ── 407 —— 下回合起，每回合{0}等级+{1}，持续{2}回合 ──
// int_args = [owner, target, 槽(官方码), 增量, 回合]（args_num=3）。
EffectResult effect_407_round_stat_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 407, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 407, args,
                     "407 布局=[owner,target,槽,增量,回合]");
    const int owner = args.int_args[0];
    const int official_slot = args.int_args[2];
    const int delta = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && rounds > 0 && delta > 0, 407, args,
                     "需要 owner∈{0,1} 增量/回合>0");
    const int slot = j125_official_slot_to_engine(official_slot);
    EFFECT_ARG_GUARD(slot >= 0, 407, args, "407 {0} 槽码超出官方码序（0-4）");
    (void)register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 407, rounds,
                                      std::vector<int>{owner, slot, delta},
                                      &j125_round_stat_up_tick);
    return EffectResult::kOk;
}

// ── 404 —— 恢复双方全部体力 ──
// int_args = [owner, target]（0 参效果，装载器注入）。
EffectResult effect_404_heal_both_full(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 404, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 404, args,
                     "404 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     404, args, "需要 owner/target ∈ {0,1}");
    (void)heal_max_hp_fraction(ctx, owner, 1);
    (void)heal_max_hp_fraction(ctx, opp, 1);
    return EffectResult::kOk;
}

// ── 541 —— 造成的攻击伤害若低于{0}则恢复自身{1}点体力 ──
// int_args = [owner, target, 阈值, 回复]（args_num=2）。989 DETECT 直系同形。
EffectResult effect_541_low_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 541, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 541, args,
                     "541 布局=[owner,target,阈值,回复]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int heal = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     541, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(threshold > 0 && heal > 0, 541, args, "需要 阈值/回复 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, target, threshold, heal](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId != target || !d.isRed || d.final <= 0) {
                return;   // 只随本次对目标的红伤判定
            }
            if (d.final >= threshold) {
                return;   // "造成的攻击伤害若低于{0}"（严格小于）
            }
            (void)g_core->heal_amount(c, owner, heal);
        });
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(407, effect_407_round_stat_up)
SKILL_EFFECT(404, effect_404_heal_both_full)
SKILL_EFFECT(541, effect_541_low_damage_heal)
