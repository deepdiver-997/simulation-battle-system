#include <utils/sbs_rand.h>
// 神谕古王（洪荒·列奥尼达神谕 4125）技能组
//
//   2399 王·宿世归泯(38248)：双方任意一方处于属性技能无效效果时，对手衰弱回合数归
//        1（+自身每层旧日之晷 +1，至多 6）；若对手不处于异常则附加 1 层宙变之殢
//   2400 王·宿世归泯(38248)：未击败对手时，消除自身当回合所使用技能的记录
//        （配合魂印"下次使用技能后再拿 1 层旧日之晷"）
//   2401 王·宿世归泯(38248)：本场战斗双方每使 1 只精灵消逝，旧日之晷触发阈值-1
//        （魂印侧读消逝数）且此技能攻击伤害+30%、吸取对手最大体力 10%
//   1167 王·宿世归泯(38248)：解除自身能力下降状态，解除成功则恢复自身全部体力
//   2398 末法迪知(29253 强化)：全属性+1；自身处于异常时己方上只在场精灵恢复 1/3
//        最大体力，不处于异常时附加 1 层宙变之殢
//   2396 王·天衍星霜(38249 五技)：每次使用后此技能攻击伤害+30%，最高+120%
//   2397 王·天衍星霜(38249 五技)：击败对手时，将对手的宙变之殢转移给自身并转化为
//        等量的星附属异常状态（星赐/星哲/星赎）
//   1249 两代五技共有：造成伤害的 X% 恢复自身体力，若对手处于异常则附加等量百分比伤害
//   2020 末法迪知(29253)：若自身处于异常状态则先制+3
#include "common.h"
#include "../oracle_runtime.h"

namespace {

// 神谕态注册键 = effect_icon 神谕版行 effect_id 2470（K1 版本行约定）
constexpr int kOracleMarkId = 2470;
constexpr int kWeaken = static_cast<int>(AbnormalStatusId::Weakness);  // 衰弱 11
constexpr int kStarBlessing = static_cast<int>(AbnormalStatusId::StarBlessing);  // 33
constexpr int kStarWisdom = static_cast<int>(AbnormalStatusId::StarWisdom);      // 34
constexpr int kStarRedemption = static_cast<int>(AbnormalStatusId::StarRedemption);  // 38

// 最大体力（本体优先逐级回退；soul_lib/common.h 同款，moves_lib 侧就地实现）
int oracle_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

bool self_has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

// ── 2020 —— 自身处于异常状态则先制+3（MOVE_RIGHT 时点）──
EffectResult effect_2020_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args ? args.int_args[0] : -1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (self_has_any_anomaly(ctx, owner)) {
        ctx->ws.preemptive_level[owner] += 3;
    }
    return EffectResult::kOk;
}

// ── 1167 —— 解除自身能力下降，解除成功则恢复全部体力 ──
EffectResult effect_1167_reverse_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !g_core || !g_core->stat_reversal
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    g_core->heal_amount(ctx, owner, std::max(1, oracle_max_hp(host)));
    return EffectResult::kOk;
}

// ── 1249 —— 造成伤害的 X% 恢复自身体力，对手处于异常则附加等量百分比伤害 ──
// （注册时点映射到 ATTACK_DAMAGE，见 skills.cpp effect_register_state）
EffectResult effect_1249_lifesteal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];   // [owner, target, X]
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int raw = ctx->ws.raw_attack_damage[owner];   // 攻伤台账是 per-side 数组
    if (raw <= 0) {
        return EffectResult::kOk;
    }
    const int heal = raw * pct / 100;
    if (heal > 0 && g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, heal);
    }
    if (heal > 0 && self_has_any_anomaly(ctx, 1 - owner) && g_core
        && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, 1 - owner, heal, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 2396 —— 天衍星霜：每次使用后此技能伤害+30%，最高+120% ──
EffectResult effect_2396_star_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args ? args.int_args[0] : -1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int uses = oracle_pub_get(ctx->plugin_storage, kOraclePubStarUses + owner) + 1;
    oracle_pub_set(ctx->plugin_storage, kOraclePubStarUses + owner, uses);
    ctx->ws.damage_add_pct[owner] += std::min(120, 30 * uses);
    return EffectResult::kOk;
}

// ── 2397 —— 天衍星霜：击败对手时转移宙变之殢 → 转化为等量星附属异常（BATTLE_AFTER_DEFEATING_OPPONENT）──
EffectResult effect_2397_transfer(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args ? args.int_args[0] : -1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int layers = oracle_pub_get(ctx->plugin_storage, kOraclePubZhie + (1 - owner));
    if (layers <= 0) {
        return EffectResult::kOk;
    }
    oracle_pub_set(ctx->plugin_storage, kOraclePubZhie + (1 - owner), 0);
    oracle_sync_freeze(ctx, 1 - owner, 0);
    if (!g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    // 每层 → 随机一种星附属异常（星赐 33 / 星哲 34 / 星赎 38），2 回合
    static const int kStars[3] = {kStarBlessing, kStarWisdom, kStarRedemption};
    for (int i = 0; i < layers; ++i) {
        g_core->apply_anomaly(ctx, owner, kStars[ctx->rng().next() % 3], /*duration=*/2, owner);
    }
    return EffectResult::kOk;
}

// ── 2398 —— 末法迪知：全属性+1；处于异常 → 己方上只在场精灵回 1/3；否则 +1 层殢 ──
EffectResult effect_2398_buff(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || !g_core) {
        return EffectResult::kOk;
    }
    if (g_core->stat_change) {
        for (int slot = 0; slot < 6; ++slot) {
            g_core->stat_change(ctx, owner, slot, 1);
        }
    }
    if (self_has_any_anomaly(ctx, owner)) {
        // 己方上只在场精灵（2398 施放前的那只）恢复其最大体力 1/3。
        // ⚠️ heal_amount 只作用在场精灵 → 上只在场的那只如今在场下，直接回体力
        //    （封回血/恢复修正不参与——口径待实测，如需走 heal 通道改这里）。
        const int prev = oracle_pub_get(ctx->plugin_storage, kOraclePubPrevAlly + owner);
        if (prev >= 0 && prev < 6) {
            ElfPet& ally = ctx->seerRobot[owner].elfPets[prev];
            if (ally.hp > 0) {
                const int max_hp = oracle_max_hp(ally);
                ally.hp = std::min(max_hp, ally.hp + std::max(1, max_hp / 3));
            }
        }
    } else {
        const int layers = oracle_pub_get(ctx->plugin_storage, kOraclePubZhie + (1 - owner));
        oracle_pub_set(ctx->plugin_storage, kOraclePubZhie + (1 - owner), std::min(5, layers + 1));
        oracle_sync_freeze(ctx, 1 - owner, std::min(5, layers + 1));
    }
    return EffectResult::kOk;
}

// ── 2399 —— 宿世归泯：属性技能无效在场上 → 对手衰弱刷新（1+日晷层，至多 6）；
//             对手不处于异常 → +1 层宙变之殢 ──
EffectResult effect_2399_refresh_weaken(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // 前提：双方任意一方处于"属性技能无效"效果（SEAL_ATTRIBUTE 活跃票）
    const bool attr_sealed = ctx->rule_center_.has_seal_kind(0, SealKind::SEAL_ATTRIBUTE,
                                                             ctx->roundCount)
                          || ctx->rule_center_.has_seal_kind(1, SealKind::SEAL_ATTRIBUTE,
                                                             ctx->roundCount);
    if (!attr_sealed) {
        return EffectResult::kOk;
    }
    // 对手的衰弱刷新：1 + 自身旧日之晷层数，至多 6 回合（没衰弱就没用）
    const int rounds = std::min(6, 1 + oracle_pub_get(ctx->plugin_storage,
                                                      kOraclePubSundial + owner));
    if (g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, opp, kWeaken) && g_core->set_anomaly_rounds) {
        g_core->set_anomaly_rounds(ctx, opp, kWeaken, rounds);
    }
    // 对手不处于任何异常 → 附加 1 层宙变之殢
    if (!self_has_any_anomaly(ctx, opp)) {
        const int layers = oracle_pub_get(ctx->plugin_storage, kOraclePubZhie + opp);
        oracle_pub_set(ctx->plugin_storage, kOraclePubZhie + opp, std::min(5, layers + 1));
        oracle_sync_freeze(ctx, opp, std::min(5, layers + 1));
    }
    return EffectResult::kOk;
}

// ── 2400 —— 未击败对手时，消除自身当回合技能使用记录（魂印下次使用再拿日晷）──
EffectResult effect_2400_clear_record(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // "未击败对手时"：对手在场那只还活着（击败了就不再清）
    ElfPet& opp = ctx->seerRobot[1 - owner].elfPets[ctx->on_stage[1 - owner]];
    if (opp.hp <= 0) {
        return EffectResult::kOk;
    }
    oracle_pub_set(ctx->plugin_storage, kOraclePubClearFirstUse + owner, 1);
    return EffectResult::kOk;
}

// ── 2401 —— 每消逝 1 只：此技能伤害+30%、吸取对手最大体力 10%（阈值-1 在魂印侧）──
EffectResult effect_2401_vanish_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int vanishes = oracle_pub_get(ctx->plugin_storage, kOraclePubVanish + owner);
    if (vanishes > 0) {
        ctx->ws.damage_add_pct[owner] += 30 * vanishes;
    }
    if (g_core && g_core->drain_hp) {
        g_core->drain_hp(ctx, /*actor=*/owner, /*target=*/opp, /*fraction_denom=*/10);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2020, effect_2020_priority);
SKILL_EFFECT(1167, effect_1167_reverse_heal);
SKILL_EFFECT(1249, effect_1249_lifesteal);
SKILL_EFFECT(2396, effect_2396_star_amp);
SKILL_EFFECT(2397, effect_2397_transfer);
SKILL_EFFECT(2398, effect_2398_buff);
SKILL_EFFECT(2399, effect_2399_refresh_weaken);
SKILL_EFFECT(2400, effect_2400_clear_record);
SKILL_EFFECT(2401, effect_2401_vanish_amp);
