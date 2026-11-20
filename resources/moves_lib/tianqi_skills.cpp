// 天启星魂 4677 技能组（星盘联动 + 星赐/星哲条件族）
//
// 官方参数（moves.side_effect / effect_info，2026-09-20 查库）：
//   2210 五技·魂·天忤太虚(28810) iconInherent：处于星赐且不处于星哲 → 转化为物理攻击；
//        处于星哲且不处于星赐 → 转化为特殊攻击；转化后系别与自身相同、威力随天赋提升
//   2207 宙始星刻(37513)：自身异常剩余回合数 -1（**加速消耗**，不是解除——星盘族判据）
//   2208 星合(37512)：星盘每转动1刻威力+15，回返期间翻倍
//   2209 星澜(37515)：星盘每转动1刻吸取对手30点体力，回返期间改为汲取
//   2218 梦觉星启(37514)×2：附加2回合星赐(33)/星哲(34)，已处于则改为全属性+1
//        （两条子句分别由 攻>特攻 / 攻<特攻 门控——官方技能文本，见函数注释）
//   802  宙始星刻(37513)：5回合免疫控制类异常
//   1060 星合(37512)：吸取对手能力提升，成功则下1回合对手攻击技能无效
//   1056 星澜(37515)：反转自身能力下降，成功则1回合对手属性技能无效
//   976  五技(28810)：消除对手回合类效果，成功则对手下1次属性技能失效
//   935  五技(28810)：自身体力高于对手 → 使对手束缚(28)
//   773  五技(28810)：自身体力低于对手 → 与对手互换体力
#include "common.h"
#include "../tianqi_runtime.h"
#include <iostream>

namespace {

constexpr int kStarBlessing = static_cast<int>(AbnormalStatusId::StarBlessing);  // 33
constexpr int kStarWisdom = static_cast<int>(AbnormalStatusId::StarWisdom);      // 34
constexpr int kBind = static_cast<int>(AbnormalStatusId::Bind);                  // 28

// ── 2210 —— 五技转化（iconInherent，SKILL_EFFECT 时点改写执行期视图）──
// 三处视图：skill_type_view（物理/特殊——calculateDamage 按它选攻防项与属性门）、
// skill_element_view（=自身系别，克制按它算）、force_involve（"系别与自身相同"→ 本系 ×1.5）。
// 威力：满天赋 180（用户 2026-09-20 口径，"一般都会刷满天赋"，写死）。
constexpr int kTransformedPower = 180;
EffectResult effect_2210_transform(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args ? args.int_args[0] : -1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const bool ci = g_core->has_active_anomaly(ctx, owner, kStarBlessing);
    const bool zhe = g_core->has_active_anomaly(ctx, owner, kStarWisdom);
    int eff_type = -1;
    if (ci && !zhe) {
        eff_type = static_cast<int>(SkillType::Physical);
    } else if (zhe && !ci) {
        eff_type = static_cast<int>(SkillType::Special);
    } else {
        return EffectResult::kOk;   // 双处于/均不处于 → 保持属性技能（无红伤，其余子句照常）
    }
    ElfPet& host = ctx->getPet(owner);
    ctx->ws.skill_type_view[owner] = eff_type;
    // 系别与自身相同：克制视图用宿主系别（本回合视图，sync 每回合重基）。
    ctx->ws.skill_element_view[owner][0] = ctx->ws.view_elementalAttributes[owner][0];
    ctx->ws.skill_element_view[owner][1] = ctx->ws.view_elementalAttributes[owner][1];
    ctx->ws.force_involve[owner] = true;   // 同系 → 本系加成（2490 同款显式开关）
    ctx->ws.skill_power_view[owner] = kTransformedPower;
    (void)host;
    return EffectResult::kOk;
}

// ── 2207 —— 自身异常剩余回合数 -1（加速消耗）──
EffectResult effect_2207_accelerate(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];   // [owner, target, N]
    if (owner < 0 || owner > 1 || delta <= 0 || !g_core || !g_core->reduce_active_anomaly_rounds) {
        return EffectResult::kOk;
    }
    g_core->reduce_active_anomaly_rounds(ctx, owner, delta);
    return EffectResult::kOk;
}

// ── 2208 —— 星合：星盘每刻威力+15，回返期间翻倍 ──
EffectResult effect_2208_dial_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int per_dial = args.int_args[2];   // [owner, target, 15]
    if (owner < 0 || owner > 1 || per_dial <= 0) {
        return EffectResult::kOk;
    }
    // ⚠️ 0 刻也要写视图：SkillView::operator+= 恒打 rewritten（变威力标记）——官方口径
    // （用户 2026-09-20）："星合的变威力就算是 0 刻也是变威力结算，暴击后要重新计算一次
    // 伤害"。没有星盘运行时（非 4677 宿主）按 dial=0 同样处理。
    const int dial = tianqi_pub_dial(ctx->plugin_storage, owner);
    const bool returning = tianqi_pub_returning(ctx->plugin_storage, owner);
    ctx->ws.skill_power_view[owner] += per_dial * dial * (returning ? 2 : 1);
    return EffectResult::kOk;
}

// ── 2209 —— 星澜：星盘每刻吸取30点体力，回返期间改为汲取 ──
// 吸取 = 对手扣、自己回（走 heal_amount，吃封回血）；回返期间"汲取"= 直接抽取
// （量翻倍、不回自身）——官方未给数值口径，按机制名分叉实现（待实测）。
EffectResult effect_2209_dial_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int per_dial = args.int_args[2];   // [owner, target, 30]
    if (owner < 0 || owner > 1 || per_dial <= 0) {
        return EffectResult::kOk;
    }
    const int dial = tianqi_pub_dial(ctx->plugin_storage, owner);
    const bool returning = tianqi_pub_returning(ctx->plugin_storage, owner);
    if (dial <= 0) {
        return EffectResult::kOk;
    }
    int amount = per_dial * dial;
    if (returning) {
        amount *= 2;
        if (g_core && g_core->fixed_damage) {
            g_core->fixed_damage(ctx, 1 - owner, amount);
        }
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->seerRobot[1 - owner].elfPets[ctx->on_stage[1 - owner]];
    if (amount > opp.hp) {
        amount = opp.hp;
    }
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    opp.hp -= amount;
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

// ── 2218 —— 附加N回合的X异常，已处于则改为全属性+N ──
// Args=[owner, target, rounds, status_id, boost]。梦觉星启的两条子句分别由
// 攻>特攻（星赐）/ 攻<特攻（星哲）门控——官方技能文本，模板本身不携带该条件，
// 这里按 status_id 分叉（33/34 才门控，其他 id 直附，保持模板通用性）。
EffectResult effect_2218_attach_or_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int status_id = args.int_args[3];
    const int boost = args.int_args[4];
    if (owner < 0 || owner > 1 || rounds <= 0 || boost <= 0) {
        return EffectResult::kOk;
    }
    if (status_id == kStarBlessing || status_id == kStarWisdom) {
        ElfPet& host = ctx->getPet(owner);
        const int atk = host.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK];
        const int spatk = host.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK];
        if (status_id == kStarBlessing && !(atk > spatk)) {
            return EffectResult::kOk;   // 星赐子句：仅攻>特攻
        }
        if (status_id == kStarWisdom && !(atk < spatk)) {
            return EffectResult::kOk;   // 星哲子句：仅攻<特攻
        }
    }
    if (g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, owner, status_id)) {
        // 已处于 → 全属性+boost（含命中，kAbilityLevelSlotCount=6，异常批次口径）
        if (!g_core->stat_change) {
            return EffectResult::kOk;
        }
        for (int slot = 0; slot < 6; ++slot) {
            g_core->stat_change(ctx, owner, slot, boost);
        }
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, owner, status_id, rounds, owner);
    }
    return EffectResult::kOk;
}

// ── 802 —— N回合免疫控制类异常（宙始星刻）──
EffectResult effect_802_immune_control(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];   // [owner, target, 5]
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 控制类异常掩码（is_control_abnormal_status 全表扫一遍）
    uint64_t mask = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
            mask |= (1ULL << id);
        }
    }
    ctx->rule_center_.grant_immune(
        owner, static_cast<int>(ImmunityType::ANOMALY), ~0ULL, mask,
        /*duration_rounds=*/rounds, ctx->roundCount, /*source_id=*/0,
        /*soul=*/false, EffectScope::ON_STAGE, /*source_slot=*/-1,
        /*counts=*/0, /*source_effect_id=*/802);
    return EffectResult::kOk;
}

// ── 1060 —— 吸取对手能力提升，成功则下N回合对手攻击技能无效 ──
EffectResult effect_1060_drain_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];   // [owner, target, 1]
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->transfer_stat_boosts || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 吸取失败（无提升/被免消除强化保护）→ 无后续
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/1060,
                       /*attribute=*/false, /*attack=*/true,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1056 —— 反转自身能力下降，成功则1回合对手属性技能无效 ──
EffectResult effect_1056_reverse_and_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];   // [owner, target, 1]
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_reversal || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // 只有"负等级翻正"算反转成功（无可反转的下降 → 不触发后续）
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/1056,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 976 —— 消除对手回合类效果，成功则对手下N次属性技能失效 ──
EffectResult effect_976_break_then_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];   // [owner, target, 1]
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || times <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->break_round_effects || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/976,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/times, /*duration_rounds=*/0, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 935 —— 自身体力高于对手则使对手束缚 ──
EffectResult effect_935_bind_if_higher(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int status_id = args.int_args[2];   // [owner, target, 28]
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    ElfPet& other = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (self.hp <= other.hp) {
        return EffectResult::kOk;   // "高于"= 严格大于
    }
    if (g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, opp, status_id, /*duration=*/-1, owner);
    }
    return EffectResult::kOk;
}

// ── 773 —— 自身体力低于对手则与对手互换体力 ──
EffectResult effect_773_swap_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    ElfPet& other = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (self.hp >= other.hp) {
        return EffectResult::kOk;   // "低于"= 严格小于
    }
    // ⚠️ 互换也要各自钳到自身最大体力（fuzz seed 17026 首曝：hp=185 > max=144——
    //   对面高上限精灵的 hp 换给被"削减体力上限"类效果压过上限的一方就越界）。
    //   官方环境堆不出 max 削减+互换同场，钳制对官方口径无行为差异；
    //   引擎全局不变式 hp ≤ 上限（reduce_max_hp_* 同款守卫）必须维持。
    //   先算后写（effect_2496_swap_tick 同款）。
    const int self_max = std::max(1, self.numericalBase[NumericalPropertyIndex::HP]);
    const int other_max = std::max(1, other.numericalBase[NumericalPropertyIndex::HP]);
    const int new_self = std::min(other.hp, self_max);
    const int new_other = std::min(self.hp, other_max);
    self.hp = new_self;
    other.hp = new_other;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2210, effect_2210_transform);
SKILL_EFFECT(2207, effect_2207_accelerate);
SKILL_EFFECT(2208, effect_2208_dial_power);
SKILL_EFFECT(2209, effect_2209_dial_drain);
SKILL_EFFECT(2218, effect_2218_attach_or_boost);
SKILL_EFFECT(802, effect_802_immune_control);
SKILL_EFFECT(1060, effect_1060_drain_boosts);
SKILL_EFFECT(1056, effect_1056_reverse_and_seal);
SKILL_EFFECT(976, effect_976_break_then_seal);
SKILL_EFFECT(935, effect_935_bind_if_higher);
SKILL_EFFECT(773, effect_773_swap_hp);
