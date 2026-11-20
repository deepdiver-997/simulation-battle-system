// D21 · 灵巢之主·索杰德尔 4599 第五技「往生虚冥叹 37088」缺失效果 1233、2108
//（夜跑 night-2，2026-09-28）
//
// 37088 链 = [797, 1233, 2108]、args [2, 20, 5, 40, 40, 1]：797=[2]（selection_seal.cpp
// 已接）、1233=[20, 5, 40]（本文件）、2108=[40, 1]（本文件）。
// 4399 图鉴（往生虚冥叹）："必中；消除对手回合类效果，消除成功{0}回合内对手无法通过
// 自身技能恢复体力；吸取对手最大体力的20%，每次使用增加5%，最高40%；交换命起、魂落的
// 层数，未击败对手自身每有1层命起下次攻击威力提升40点，每有1层魂落下次攻击先制+1"。
//
// 1233 "吸取对手最大体力的{0}%，每次使用增加{1}%，最高{2}%"：
//   - 递增档 = 2396 同款：uses 计数存 plugin_storage，吸取比例 = min(20+5×(uses-1), 40)
//     （首用 20%，每次+5%，40% 封顶）；量 = 对手 max_hp × pct/100；
//   - 吸取 = deal_pink_damage(PERCENT_VALUE) + heal_amount 等量回自（1249 同款）；
//     与攻击伤害无耦合 → SKILL_EFFECT 直发（无需桥）。
// 2108 "交换命起、魂落的层数，未击败对手自身每有1层命起下次攻击威力提升{0}点，
//   每有1层魂落下次攻击先制+{1}"：
//   - 层数走共享层 effects/lingchao_layers.h（与魂印侧同源，和恒 4）；SKILL_EFFECT 时
//     交换（lingchao_set_layers(魂落, 命起)）；
//   - "未击败"按伤害落地后判 → AFTER_ACTION 桥（582 同款）：对手存活 → 按交换后的
//     层数武装两组 charges（命起层数→威力池、魂落层数→先制池，覆盖式重挂）；
//   - 消耗：威力 = FIRST_SKILL_EFFECT 常驻节点——持有方下次攻击技（攻击技能限定）在
//     威力视图物化后（ON_SKILL_HIT 物化、SKILL_EFFECT 桶内效果可改，862 同款时点）
//     skill_power_view += {0}×命起层，一次性清池；先制 = FIRST_MOVE_RIGHT 常驻节点，
//     下次攻击技 preemptive += {1}×魂落层，一次性清池。
//
// 场景回归：test/scenario/scenario_370_lingchao_4599_1233_2108.cpp。

#include "common.h"

#include <effects/lingchao_layers.h>

namespace {

constexpr int kLingchaoMarkId = 2193;            // 灵巢之主魂印（effect_icon.effect_id）
constexpr int k1233UsesKeyBase = 990123310;      // 1233 已用次数（owner 侧）
constexpr int k2108PowerKeyBase = 990210810;     // 威力提升 charges（owner 侧）
constexpr int k2108PrioKeyBase = 990210820;      // 先制提升 charges（owner 侧）
constexpr int k2108WatchKeyBase = 990210830;     // 2108 常驻节点已注册旗标（owner 侧）

int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1233 —— 吸取对手最大体力的{0}%，每次使用增加{1}%，最高{2}% ──
EffectResult effect_1233_ramp_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->deal_pink_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || base <= 0 || step <= 0
        || cap <= 0) {
        return EffectResult::kOk;
    }
    const int key = k1233UsesKeyBase + owner;
    const int uses = storage_int(ctx, key) + 1;
    ctx->plugin_storage[key] = uses;
    const int pct = std::min(base + step * (uses - 1), cap);
    const int drain = max_hp_of(ctx, target) * pct / 100;
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::PERCENT_VALUE, owner);
    g_core->heal_amount(ctx, owner, drain);
    return EffectResult::kOk;
}

// ── 2108 的 AFTER_ACTION 桥：未击败对手 → 按交换后层数武装 charges ──
EffectResult effect_2108_arm_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int power_per = args.int_args[2];
    const int prio_per = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手：不武装
    }
    const int slot = lingchao_find_slot(ctx, owner, kLingchaoMarkId);
    if (slot < 0) {
        return EffectResult::kOk;   // 非灵巢/无层载体
    }
    ctx->plugin_storage[k2108PowerKeyBase + owner] =
        lingchao_layers(ctx, owner, slot, kLingchaoMingqiKey);
    ctx->plugin_storage[k2108PrioKeyBase + owner] =
        lingchao_layers(ctx, owner, slot, kLingchaoHunluoKey);
    (void)power_per;
    (void)prio_per;
    return EffectResult::kOk;
}

// ── 2108 的 SKILL_EFFECT 常驻节点：威力视图 += {0}×命起层（攻击技一次性清池）──
EffectResult effect_2108_power_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int power_per = args.int_args[1];
    if (owner < 0 || owner > 1 || !executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int key = k2108PowerKeyBase + owner;
    const int layers = storage_int(ctx, key);
    if (layers <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = 0;
    ctx->ws.skill_power_view[owner] += power_per * layers;   // SkillView::+=（862 同款）
    return EffectResult::kOk;
}

// ── 2108 的 MOVE_RIGHT 常驻节点：先制 += {1}×魂落层（攻击技一次性清池）──
EffectResult effect_2108_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int prio_per = args.int_args[1];
    if (owner < 0 || owner > 1 || !selected_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;
    }
    const int key = k2108PrioKeyBase + owner;
    const int layers = storage_int(ctx, key);
    if (layers <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[key] = 0;
    ctx->ws.preemptive_level[owner] += prio_per * layers;
    return EffectResult::kOk;
}

// ── 2108 —— 交换命起、魂落的层数 + 未击败对手武装下次攻击骑乘 ──
EffectResult effect_2108_swap_layers(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int power_per = args.int_args[2];
    const int prio_per = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // ① 交换命起/魂落（交换不改和，和恒 4）
    const int slot = lingchao_find_slot(ctx, owner, kLingchaoMarkId);
    if (slot >= 0) {
        const int mingqi = lingchao_layers(ctx, owner, slot, kLingchaoMingqiKey);
        const int hunluo = lingchao_layers(ctx, owner, slot, kLingchaoHunluoKey);
        lingchao_set_layers(ctx, owner, slot, hunluo, mingqi);
    }
    // ② "未击败对手"的骑乘武装：AFTER_ACTION 桥（伤害落地后判存活），常驻消费节点
    //    各注册一次（guard）。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 2108, 1,
                              {owner, target, power_per, prio_per}, &effect_2108_arm_tick);
    }
    if (!ctx->plugin_storage.count(k2108WatchKeyBase + owner)) {
        ctx->plugin_storage[k2108WatchKeyBase + owner] = 1;
        register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, 2108,
                              /*rounds=*/999, {owner, power_per}, &effect_2108_power_tick);
        register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 2108,
                              /*rounds=*/999, {owner, prio_per}, &effect_2108_priority_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1233, effect_1233_ramp_drain);
SKILL_EFFECT(2108, effect_2108_swap_layers);
