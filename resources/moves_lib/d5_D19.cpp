// D19 · 箫澈 4496 第五技「箫·残夜悠悠 36547」缺失效果 1782、1870、1962、1963
//（夜跑 night-2，2026-09-28）
//
// 36547 链 = [1782, 1870, 1962, 1963]、全部 args_num=0（side_effect_arg 空）。
// 4399 图鉴（箫·残夜悠悠）："必中；自身存在护盾则先制+2；自身存在护罩则造成的攻击
// 伤害翻倍；未击败对手则消除对手回合类效果；未击败对手则消除对手能力提升状态"。
// ★ 1962/1963 与 D28 妙时天女共享——后做方直接复用本实现，勿重复注册。
//
// 1782 "自身存在护盾则先制+2"：
//   - MOVE_RIGHT 常驻节点（首用后武装）：所选技==36547 且 shield_bank_.total()>0 →
//     preemptive_level += 2（610/仐 条件先制族同判）；
//   - ⚠️ 结构性偏差记档：先手权结算先于 SKILL_EFFECT，本技能效果链没有更早的武装点
//     → **每场战斗第 1 次使用不加先制**（官方应加），第 2 次起正常。与 1098 的
//     暴击首用偏差同根源，待拍板。
// 1870 "自身存在护罩则造成的攻击伤害翻倍"：
//   - hood_bank_.total()>0 → ws.damage_add_pct += 100（通用加法通道，1178 同款；
//     翻倍=+100%，与 776/1959 一致）；SKILL_EFFECT 时点先于伤害，当次生效。
// 1962/1963 "未击败对手则消除对手回合类效果 / 消除对手能力提升状态"：
//   - "未击败"须在伤害落地后判 → AFTER_ACTION 桥（582 同款，FIRST/SECOND 双侧
//     rounds=1，只活在用技当回合）：对手在场宠 hp>0 才消除；
//   - 消除回合类 = break_round_effects（1032 同款）；消除提升 = clear_stat_boosts
//     （返回>0 即有清除，1217 同款；两条各自独立判断）。
//
// 场景回归：test/scenario/scenario_350_xiaochе_4496.cpp。

#include "common.h"

namespace {

constexpr int kSkillId = 36547;             // 箫·残夜悠悠
constexpr int k1782ArmedKeyBase = 990178210; // 1782 常驻节点已注册旗标（owner 侧）

// 本回合 owner 所选技能是否为箫·残夜悠悠（MOVE_RIGHT 时 roundChoice 已写入）。
bool selected_is_36547(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    if (!ctx || owner < 0 || owner > 1 || ctx->roundChoice[owner][0] != kActionSelectSkill) {
        return false;
    }
    const int slot = ctx->roundChoice[owner][1];
    const auto& skills = ctx->getPet(owner).skills;
    if (slot < 0 || slot >= static_cast<int>(skills.size())) {
        return false;
    }
    return skills[static_cast<std::size_t>(slot)].id == kSkillId;
}

// ── 1782 的 MOVE_RIGHT 节点：有盾 → 先制 +2 ──
EffectResult effect_1782_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !selected_is_36547(ctx, owner)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).shield_bank_.total() > 0) {
        ctx->ws.preemptive_level[owner] += 2;
    }
    return EffectResult::kOk;
}

// ── 1782 —— 自身存在护盾则先制+2（武装节点）──
EffectResult effect_1782_shield_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = k1782ArmedKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 常驻节点只注册一次
    }
    ctx->plugin_storage[key] = 1;
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1782,
                          /*rounds=*/999, {owner}, &effect_1782_priority_tick);
    return EffectResult::kOk;
}

// ── 1870 —— 自身存在护罩则造成的攻击伤害翻倍 ──
EffectResult effect_1870_hood_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).hood_bank_.total() <= 0) {
        return EffectResult::kOk;   // 无护罩：不翻倍
    }
    ctx->ws.damage_add_pct[owner] += 100.0f;   // 翻倍 = 通用增伤 +100%
    return EffectResult::kOk;
}

// ── 1962/1963 的 AFTER_ACTION 桥：未击败对手 → 消除 ──
EffectResult effect_1962_break_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手：不消除
    }
    g_core->break_round_effects(ctx, target);
    return EffectResult::kOk;
}

EffectResult effect_1963_clear_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手：不消除
    }
    (void)g_core->clear_stat_boosts(ctx, target);
    return EffectResult::kOk;
}

// ── 1962 —— 未击败对手则消除对手回合类效果（AFTER_ACTION 桥武装）──
EffectResult effect_1962_break_if_not_defeated(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1962, 1, {owner, target},
                              &effect_1962_break_tick);
    }
    return EffectResult::kOk;
}

// ── 1963 —— 未击败对手则消除对手能力提升状态（AFTER_ACTION 桥武装）──
EffectResult effect_1963_clear_if_not_defeated(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1963, 1, {owner, target},
                              &effect_1963_clear_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1782, effect_1782_shield_priority);
SKILL_EFFECT(1870, effect_1870_hood_double);
SKILL_EFFECT(1962, effect_1962_break_if_not_defeated);
SKILL_EFFECT(1963, effect_1963_clear_if_not_defeated);
