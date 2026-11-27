// E2 · 消回合/消强成功族 —— 453×43 / 915×40 / 979×51 / 1219×51（2026-09-29，worktree ../agents/e-E2）
//
// 家族形状（批 6 的姊妹批，selection_seal.cpp 797/889 同门槛）：消除类效果按结果分支——
//   SUCCESS（真的清掉了东西）才执行后续子句；NO_EFFECTS（对方本来就没有）不给。
//
// 453（args_num=1）"消除对手能力提升状态，若消除成功，则对手{0}"（{0}=异常码）：
//   - ⚠️ 门槛是**消强**（clear_stat_boosts 返回清掉个数 >0），非消回合——归本家族按引用
//     聚类，官方模板字面如此；868（消强→对手先制-）同门槛先例。
//   - {0} 为官方异常编号（0=麻痹…9=石化，is_valid_abnormal_status_id 校验），走
//     apply_anomaly 现代通道（889 字面同款，免疫/抗性/弹控全继承）。
// 915（args_num=3）"消除对手回合类效果，消除成功则自身下{0}回合攻击有{1}%使对手{2}"：
//   - break_round_effects SUCCESS → NextRounds 双桶窗口（ON_SKILL_HIT × FIRST/SECOND，
//     470 同款 tick：executed_skill_is_attack + roll_skill_percent + apply_anomaly）。
//   - "下{0}回合"= NextRounds 起点（本回合不算）→ 不做 470 的"当次补掷"（470 是
//     "{0}回合内"含当回合才双写）。
// 979（args_num=2）"消除对手回合类效果，消除成功则自身下{0}回合所有技能先制+{1}"：
//   - break_round_effects SUCCESS → register_next_rounds_effect（MOVE_RIGHT 单桶，双方
//     通用）+ 共享时点效果体 effect_move_right_priority_bonus（843/499/868/1123 同款）。
// 1219（args_num=0）"消除对手回合类效果，消除成功则下回合受到的伤害转化为自身体力"：
//   - break_round_effects SUCCESS → EVENT_TAKE_DAMAGE watcher（1188 受击窗口同款）。
//   - "下回合"：watcher 窗口 duration=2（覆盖 R..R+1，cleanup 按 current-register>=duration
//     清），lambda 内过滤 roundCount<=R 的伤害（本回合不算）。
//   - 转化口径：ev.amount = 实际扣血（攻击伤害与粉伤两条 emit 路径都发）→ 1:1 全额
//     heal_amount（args_num=0 无比例参数，按字面全额；含粉伤为字面口径记档）。
//
// 前置三查（2026-09-29）：①内核无 979/1219/915/453 硬编码（防 697 式假阳性——grep
// skills.cpp/battle_primitives.cpp 无命中）②effect_meta 无穿透分类 ③全库无既有实现。
//
// 场景回归：test/scenario/scenario_470_break_round_family.cpp。

#include "common.h"

namespace {

// ── 453 —— 消除对手能力提升状态，若消除成功，则对手{0} ──
EffectResult effect_453_break_boosts_then_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts
        || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;   // "消除成功"门槛（无提升/被免消除挡下 → 不给异常）
    }
    g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 915 窗口 tick —— 攻击命中（结构性门控：ON_SKILL_HIT 桶只在非 SKILL_INVALID 时跑）
//      + 攻击技能限定 + 概率掷点 → 施加异常（470 同款）──
EffectResult effect_915_hit_anomaly_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (!executed_skill_is_attack(ctx, self) || !roll_skill_percent(*ctx, 915, 0, self, opp, chance)) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), self);
    return EffectResult::kOk;
}

// ── 915 —— 消除对手回合类效果，消除成功则自身下{0}回合攻击有{1}%使对手{2} ──
EffectResult effect_915_break_then_hit_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 对手本来就没有回合类效果 → 子句不触发
    }
    // "下{0}回合"= NextRounds（本回合不算，无 470 式当次补掷）；自身攻击命中才有意义
    // → 双桶（先/后手都注册，tick 里再判攻击技能）
    for (State st : {State::BATTLE_FIRST_ON_SKILL_HIT, State::BATTLE_SECOND_ON_SKILL_HIT}) {
        register_next_rounds_effect(ctx, st, owner, 915, rounds,
                                    {owner, opp, args.int_args[3], args.int_args[4]},
                                    &effect_915_hit_anomaly_tick);
    }
    return EffectResult::kOk;
}

// ── 979 —— 消除对手回合类效果，消除成功则自身下{0}回合所有技能先制+{1} ──
EffectResult effect_979_break_then_priority_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0 || bonus <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不给先制
    }
    // 1123（吸取成功→下N回合先制+）同款窗口；bonus 累加走共享先制时点效果体
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 979, rounds,
                                {owner, opp, bonus}, &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 1219 —— 消除对手回合类效果，消除成功则下回合受到的伤害转化为自身体力 ──
EffectResult effect_1219_break_then_damage_to_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->break_round_effects
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 无转化
    }
    // "下回合"：watcher 注册于 R、窗口 duration=2（有效回合 [R, R+1]，cleanup 按
    // current-register>=duration 清），lambda 内过滤 R 的伤害 → 实际转化只发生在 R+1。
    const int r0 = ctx->roundCount;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, r0, /*duration_rounds=*/2, /*once=*/false,
        [owner, r0](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || !g_core || !g_core->heal_amount) {
                return;
            }
            if (c->roundCount <= r0) {
                return;   // "下回合"：本回合（消除当回合）受到的伤害不转化
            }
            if (ev.amount > 0) {
                g_core->heal_amount(c, owner, ev.amount);   // 1:1 全额转化（字面口径）
            }
        },
        WatcherScope::TEAM);   // ⚠️ 跨回合窗口必须 TEAM——ON_STAGE 的 valid_id 判定会让
                               // watcher 活不过回合交界（1188 受击窗口同款选型）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(453, effect_453_break_boosts_then_anomaly)
SKILL_EFFECT(915, effect_915_break_then_hit_anomaly_window)
SKILL_EFFECT(979, effect_979_break_then_priority_up)
SKILL_EFFECT(1219, effect_1219_break_then_damage_to_heal)
