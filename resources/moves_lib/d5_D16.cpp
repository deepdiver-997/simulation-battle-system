// D 组批产 · D16（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   镇世·乔特鲁德 4186 五技缺口四效果（760 两技共用已有；1187 已由 burn_trio 实现）：
//     1670 命中后{0}%令对手{1}，对手为自身天敌时概率提升{2}%，未触发则消除对手
//          回合类效果（34900 切片 [50,7,30]；{1}=7=疲惫）
//     833  当回合若未击败对手则回合结束时吸取对手最大体力的1/{0}（34900 切片 [3]）
//     840  击败对手则对方下只精灵出战时己方在场精灵吸取其最大体力的1/{0}
//          （34900 切片 [3] / 37295 切片 [2]）
//     1709 附加对手最大体力1/{0}的百分比伤害，未击败对手则恢复自身等量体力值
//          （37295 切片 [3]）
//
// 实现要点：
//   · 1670：概率 = {0} + (is_predator ? {2} : 0)，经 apply_anomaly_checked 申报
//     （亮节闸门管辖，掷点在闸门改写后）；未落地（被免/抗/弹/未中）→
//     break_round_effects 兜底消回合（1187 同族结构 + 天敌加成）。
//   · 833：AFTER_ACTION 桥（对手存活=未击败）→ OLD_ROUND_END_1 一次性节点
//     drain_hp（击败路径也必跑的共享桶，1386 同理由不用 BATTLE_ROUND_END）。
//   · 840：AFTER_ACTION 桥（对手 hp<=0=当回合击败）→ 武装 + EVENT_ENTER_STAGE
//     watcher（TEAM、接线一次、消费一次）→ 对方下只登场时 drain_hp（actor=己方
//     在场侧，drain 原语内自取双方在场宠）。batch_b 67 同骨架、吸取版。
//   · 1709：SKILL_EFFECT 时点 deal_pink_damage(PERCENT_VALUE, 对手最大体力/{0})，
//     以目标血差记实扣值；AFTER_ACTION 桥判未击败（对手存活）→ heal_amount 等量回自。

#include "common.h"

#include <any>
#include <memory>
#include <vector>

namespace {

constexpr int k1709DealtKey = 17090001;   // plugin_storage：1709 本回合实扣粉伤（int）
constexpr int k840ArmKey = 84000001;      // plugin_storage：840 击败武装（int 0/1）
constexpr int k840WatchKey = 84000002;    // plugin_storage：840 watcher 接线旗标（int）

int storage_int(const BattleContext* ctx, int key) {
    if (!ctx) {
        return 0;
    }
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

void register_once_this_round(BattleContext* ctx, State trigger, int owner, int effect_id,
                              std::vector<int> ints, EffectFn fn) {
    Effect e(effect_id, 0, owner, 1, EffectArgs(std::move(ints)), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, 1, ctx->roundCount);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
}

void register_after_action_bridge(BattleContext* ctx, int owner, int effect_id,
                                  std::vector<int> ints, EffectFn fn) {
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_once_this_round(ctx, st, owner, effect_id, ints, fn);
    }
}

// ── 1670 概率疲惫（天敌加成）→ 未触发消回合；Args=[owner, opp, chance, anomaly, bonus] ──
EffectResult effect_1670_fatigue_or_break(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->apply_anomaly_checked || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int predator_bonus = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || chance < 0 || predator_bonus < 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (is_predator(ctx, owner)) {
        chance += predator_bonus;   // 对手为自身天敌
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance, ChanceSource::Skill);
    const bool landed = r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
    if (landed) {
        return EffectResult::kOk;
    }
    (void)g_core->break_round_effects(ctx, opp);   // 未触发 → 消除对手回合类效果
    return EffectResult::kOk;
}

// ── 833 当回合未击败 → 回合结束时吸取对手最大体力 1/{n} ────────────────────────
EffectResult effect_833_round_end_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    g_core->drain_hp(ctx, self, opp, denom);
    return EffectResult::kOk;
}

EffectResult effect_833_undeathed_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 当回合击败了对手 → 不吸取
    }
    register_once_this_round(ctx, State::BATTLE_OLD_ROUND_END_1, self, 833,
                             {self, opp, denom}, &effect_833_round_end_drain);
    return EffectResult::kOk;
}

// ── 840 击败 → 对方下只出战时己方在场吸取其最大体力 1/{n} ──────────────────────
void effect_840_enter_drain(BattleContext* ctx, const BattleEvent& ev, int self,
                            int opp, int denom) {
    if (!ctx || ev.actor != opp) {
        return;
    }
    const int* armed = nullptr;
    const auto it = ctx->plugin_storage.find(k840ArmKey + self);
    if (it != ctx->plugin_storage.end()) {
        armed = std::any_cast<int>(&it->second);
    }
    if (!armed || *armed == 0) {
        return;
    }
    ctx->plugin_storage[k840ArmKey + self] = 0;   // "下只出战"一次
    if (g_core && g_core->drain_hp) {
        g_core->drain_hp(ctx, self, opp, denom);   // actor=己方在场侧
    }
}

EffectResult effect_840_kill_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp > 0) {
        return EffectResult::kOk;   // 当回合未击败 → 不武装
    }
    ctx->plugin_storage[k840ArmKey + self] = 1;
    if (storage_int(ctx, k840WatchKey + self) == 0) {
        ctx->plugin_storage[k840WatchKey + self] = 1;
        // TEAM 直连注册（valid_id 默认 0 的 ON_STAGE 永不投递坑见 d5_D3.cpp 记档）。
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [self, opp, denom](BattleContext* c, const BattleEvent& ev) {
                effect_840_enter_drain(c, ev, self, opp, denom);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

EffectResult effect_840_drain_next_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    register_after_action_bridge(ctx, self, 840, {self, opp, denom},
                                 &effect_840_kill_bridge);
    return EffectResult::kOk;
}

// ── 1709 附加最大体力 1/{n} 百分比伤；未击败 → 等量回自 ────────────────────────
EffectResult effect_1709_undeathed_heal_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        ctx->plugin_storage[k1709DealtKey + self] = 0;
        return EffectResult::kOk;   // 击败了对手 → 不回血
    }
    const int dealt = storage_int(ctx, k1709DealtKey + self);
    ctx->plugin_storage[k1709DealtKey + self] = 0;
    if (dealt > 0) {
        g_core->heal_amount(ctx, self, dealt);
    }
    return EffectResult::kOk;
}

EffectResult effect_1709_pink_and_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int opp_slot = ctx->on_stage[opp];
    const int opp_max =
        ctx->seerRobot[opp].elfPets[opp_slot].numericalBase[NumericalPropertyIndex::HP];
    const int amount = opp_max / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    const int hp_before = ctx->seerRobot[opp].elfPets[opp_slot].hp;
    (void)g_core->deal_pink_damage(ctx, opp, amount, DamageKind::PERCENT_VALUE, self);
    const int dealt = hp_before - ctx->seerRobot[opp].elfPets[opp_slot].hp;
    ctx->plugin_storage[k1709DealtKey + self] = dealt > 0 ? dealt : 0;
    // 未击败判定交给 AFTER_ACTION 桥（技能效果时点早于伤害/死亡，此处不知胜负）。
    register_after_action_bridge(ctx, self, 1709, {self, opp},
                                 &effect_1709_undeathed_heal_bridge);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1670, effect_1670_fatigue_or_break);
SKILL_EFFECT(833, effect_833_undeathed_bridge);
SKILL_EFFECT(840, effect_840_drain_next_on_kill);
SKILL_EFFECT(1709, effect_1709_pink_and_heal);
