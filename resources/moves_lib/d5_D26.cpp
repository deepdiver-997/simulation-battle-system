// D26 · 无极圣武 4800 第五技「圣武·虚极拓世 38088」缺失效果 1395、2343、1709、2252
//（夜跑 night-2，2026-09-28）
//
// 38088 链 = [1395, 2343, 776, 1709, 2252]、args [1, 3, 2]：1395=[]（本文件）、
//   2343=[]（本文件）、776=[1]（round_window.cpp 已接）、1709=[3]（本文件）、2252=[2]（本文件）。
// 4399 图鉴（圣武·虚极拓世）："必中；若先出手则必定打出致命一击；对手每处于1回合的
// 威怯1回合内令对手技能无效；下1回合自身造成的攻击伤害翻倍；附加对手最大体力1/3的
// 百分比伤害，未击败对手则恢复自身等量体力值；击败对手后自身免疫下2次受到的异常状态"。
//
// 1395 "若先出手则必定打出致命一击"：
//   - SKILL_EFFECT 时点判 acting_side_is_first（先手权已定），成立则双写：
//     ① ctx->crit_happened[owner] = true（暴击旗标，下游暴击消费方可见）；
//     ② ws.damage_add_pct += 100（伤害×2，通用增伤通道——暴击倍率 cached_crit_damage=200
//        的等价物）。
//   - ⚠️ 口径记档：不走 ws.must_crit（BEFORE_SKILL_HIT 写入后伤害未按 2 倍结算，成因
//        未查清，见 git 历史）；本实现的差异 = 不触发"暴击破防"（双防重置）——该时点
//        在 ATTACK_DAMAGE 内、效果无法触及。若需完整暴击，需内核在 ①.5 前暴露
//        "条件必暴"旗标，记台账。
// 2343 "对手每处于1回合的威怯1回合内令对手技能无效"：
//   - 威怯读对手 on_stage_storage[kWujiWeiqieKey]（wuji_shengwu_runtime.h 发布槽，
//     绝对失效回合模型，剩余 = end - roundCount）；封 = SEAL_ALL（攻/属都无效）
//     duration = 威怯剩余回合（每 1 回合威怯换 1 回合无效）。
// 1709 "附加对手最大体力1/{0}的百分比伤害，未击败对手则恢复自身等量体力值"：
//   - "未击败"按伤害落地后判 → AFTER_ACTION 桥（582 同款）：对手存活 →
//     deal_pink_damage(PERCENT_VALUE, max_hp/denom) + heal_amount 等量回自（1249 同款）。
// 2252 "击败对手后自身免疫下{0}次受到的异常状态"：
//   - 同桥：对手已倒 → grant_immunity(ANOMALY, counts={0})（2087 次数型同款）。
//
// 场景回归：test/scenario/scenario_420_wuji_4800_1395_2343_1709_2252.cpp。

#include "common.h"

#include "../wuji_shengwu_runtime.h"

namespace {

constexpr int k1709BridgeKeyBase = 990170910;   // 1709 桥已注册旗标（owner 侧，防重挂）
constexpr int k2252BridgeKeyBase = 990225210;   // 2252 桥已注册旗标（owner 侧）

int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1395 —— 若先出手则必定打出致命一击（旗标 + 伤害翻倍双写）──
EffectResult effect_1395_first_mover_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手：不满足"先出手"
    }
    ctx->crit_happened[owner] = true;          // 暴击旗标（下游消费方可见）
    ctx->ws.damage_add_pct[owner] += 100.0f;   // 伤害 ×2（通用增伤通道，1178 同款）
    return EffectResult::kOk;
}

// ── 2343 —— 对手每处于1回合的威怯1回合内令对手技能无效 ──
EffectResult effect_2343_weiqie_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int opp_slot = ctx->on_stage[target];
    auto& opp_storage = ctx->seerRobot[target].elfPets[opp_slot].on_stage_storage;
    if (!wuji_mark_active(opp_storage, kWujiWeiqieKey, ctx->roundCount)) {
        return EffectResult::kOk;   // 对手没有生效中的威怯
    }
    const int end = any_int_or(opp_storage[kWujiWeiqieKey]);
    const int rounds = end - ctx->roundCount;
    if (rounds <= 0) {
        return EffectResult::kOk;
    }
    // "技能无效" = 攻/属全封（SEAL_ALL），回合型 = 威怯剩余回合
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/2343,
                       /*attribute=*/true, /*attack=*/true, /*count=*/0,
                       /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1709 的 AFTER_ACTION 桥：未击败 → 吸 1/denom 最大体力并等量回自 ──
EffectResult effect_1709_drain_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手：不恢复
    }
    const int drain = max_hp_of(ctx, target) / denom;
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::PERCENT_VALUE, owner);
    g_core->heal_amount(ctx, owner, drain);
    return EffectResult::kOk;
}

// ── 1709 —— 附加对手最大体力1/{0}的百分比伤害，未击败则恢复自身等量体力值 ──
EffectResult effect_1709_drain_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int key = k1709BridgeKeyBase + owner;
    std::cerr << "[probe-1709arm] ran once_flag=" << storage_int(ctx, key) << "\n";
    if (storage_int(ctx, key) == 0) {
        ctx->plugin_storage[key] = 1;
        for (State st : {State::BATTLE_FIRST_AFTER_ACTION,
                         State::BATTLE_SECOND_AFTER_ACTION}) {
            register_round_effect(ctx, st, owner, 1709, 1,
                                  {owner, target, denom}, &effect_1709_drain_heal_tick);
        }
    }
    return EffectResult::kOk;
}

// ── 2252 的 AFTER_ACTION 桥：击败对手 → 免疫下{0}次异常 ──
EffectResult effect_2252_immune_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp > 0) {
        return EffectResult::kOk;   // 未击败：不武装
    }
    // 次数型全异常免疫（2087 grant_immunity counts 型同款）
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, /*coverage=*/~0ULL,
                        /*anomaly_mask=*/0, /*duration_rounds=*/0,
                        /*source_id=*/2252, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/counts,
                        /*source_effect_id=*/2252);
    return EffectResult::kOk;
}

// ── 2252 —— 击败对手后自身免疫下{0}次受到的异常状态 ──
EffectResult effect_2252_immune_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || counts <= 0) {
        return EffectResult::kOk;
    }
    const int key = k2252BridgeKeyBase + owner;
    if (storage_int(ctx, key) == 0) {
        ctx->plugin_storage[key] = 1;
        for (State st : {State::BATTLE_FIRST_AFTER_ACTION,
                         State::BATTLE_SECOND_AFTER_ACTION}) {
            register_round_effect(ctx, st, owner, 2252, 1,
                                  {owner, target, counts}, &effect_2252_immune_tick);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1395, effect_1395_first_mover_crit);
SKILL_EFFECT(2343, effect_2343_weiqie_seal);
SKILL_EFFECT(1709, effect_1709_drain_heal);
SKILL_EFFECT(2252, effect_2252_immune_on_kill);
