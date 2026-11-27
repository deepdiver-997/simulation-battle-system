// 光之惩戒·英卡洛斯 3577 技能组（moves_lib）——批 9 补全 9 个缺口效果
//
// 官方文本（DB effect_info 表，arg_count 来自 side_effect 表；参数按 arg_count 顺序切分）：
//   433  {0}回合内每回合{1}                                  —— 7 参 585 码序[回合,攻,防,特攻,特防,速,命中]
//   511  {0}%概率威力翻倍 体力低于1/{1}时概率增加{2}%          —— [20,2,40]
//   699  无视攻击免疫效果 —— **内核穿透凭证**（loader 特判并入 ws.attack_credential，
//        skills.cpp Penetration 分支），不走 SKILL_EFFECT；勿再注册（死代码）。
//   769  若对手不处于异常状态则造成的攻击伤害额外提升{0}%       —— [50]
//   1040 当回合若未击败对手则{0}%令对手{1}，未触发则令自身全属性+{2} —— [80,7,1]
//   1044 吸取对手能力提升状态，吸取成功则下{0}回合造成的伤害翻倍 —— [2]
//   1188 {0}回合内自身受到攻击则使对手{1}，未触发则吸取对手最大体力的1/{2} —— [3,7,3]
//   1255 {0}回合内每回合使用技能则{1}%令对手{2}，未触发则附加对手最大体力1/{3}的百分比伤害 —— [3,100,20,3]
//   1258 造成的攻击伤害若高于{0}则自身免疫下{1}次受到的异常状态 —— [280,1]
//
// 技能装配（hide_moves/learnable，3577）：
//   31836 碎梦袭[1044] / 31837 幻影双刃[511] / 31844 璨星断刃[1297,699,925] /
//   31845 满月破碎[786] / 31846 天河圣芒斩[1048,769,1258] / 31847 光·断罪裁决[905,1128,1040] /
//   26031 光之惩戒[191,1255,1188] / 26032 无始源光[585,433,971,694,776]
//
// 口径备注（2026-09-24 批 9，v1 拍板见 docs_local/docs/06-交接/未来链接与克制锁-交接.md）：
//   - 1040/1188/1255 的"未触发"= 异常没有成功落上（免疫/抗性/反弹都算）；1255 按回合独立结算。
//   - 1188 的"使对手"= 施加给**攻击者**（受到谁攻击就给谁）。
//   - 1258 的"高于{0}"为严格大于（与魂印 ② 的 280 同一族）；免疫走 counts=1 次数票。
//   - 699 与魂印 1164 锁的关系：锁先把 0 抬到 2，699 的"0→1"只在无锁时可见（两者独立）。
#include <iostream>
#include "common.h"

namespace {

constexpr int kOfficialAnomalyMax = 43;   // 与 abnormal-types.h kOfficialAbnormalStatusMaxId 同值

// 585 码序 → 引擎能力槽：0攻→0、1防→2、2特攻→1、3特防→3、4速→4、5命中→5（stat_dispel 同表）。
constexpr int kKingStatMap[6] = {0, 2, 1, 3, 4, 5};

bool foe_has_no_anomaly(BattleContext* ctx, int foe) {
    if (!ctx || !g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAnomalyMax; ++id) {
        if (g_core->has_active_anomaly(ctx, foe, id)) {
            return false;
        }
    }
    return true;
}

bool mover_is(BattleContext* ctx, int owner) {
    if (!ctx) {
        return false;
    }
    if (ctx->currentState == State::BATTLE_FIRST_ACTION_END
        || ctx->currentState == State::BATTLE_FIRST_BEFORE_SKILL_HIT) {
        return !ctx->is_second_mover(0) ? owner == 0 : owner == 1;
    }
    if (ctx->currentState == State::BATTLE_SECOND_ACTION_END
        || ctx->currentState == State::BATTLE_SECOND_BEFORE_SKILL_HIT) {
        if (ctx->is_second_mover(0) == ctx->is_second_mover(1)) {
            return false;
        }
        return ctx->is_second_mover(0) ? owner == 0 : owner == 1;
    }
    return false;
}

// ── 433 {0}回合内每回合：自家六项能力各 ±flag（585 码序 7 参）──
EffectResult encarlos_433_round_stats(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 8 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    for (int i = 0; i < 6; ++i) {
        const int delta = args.int_args[2 + i];
        const int stat = kKingStatMap[i];
        if (delta > 0 && g_core->stat_change) {
            g_core->stat_change(ctx, self, stat, delta);
        } else if (delta < 0 && g_core->stat_drop) {
            g_core->stat_drop(ctx, self, stat, -delta, self);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_433_round_stat_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 9) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 节点载荷：[self, foe, 攻,防,特攻,特防,速,命中]（首二位是效果体绑定约定）。
    const std::vector<int> payload{self, args.int_args[1], args.int_args[3], args.int_args[4],
                                   args.int_args[5], args.int_args[6], args.int_args[7],
                                   args.int_args[8]};
    // 首跳即时生效（"N回合内"含使用回合；ROUND_START 桶在本回合已跑过，窗口注册只能
    // 覆盖到下一回合起）→ 立刻结一跳，剩余 rounds-1 跳从**下一回合**起走窗口
    // （register_next_rounds_effect：起点显式 current+1，本回合不算）。
    encarlos_433_round_stats(ctx, EffectArgs(payload));
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 433, rounds - 1,
                                    payload, &encarlos_433_round_stats);
    }
    return EffectResult::kOk;
}

// ── 511 {0}%概率威力翻倍，体力低于 1/{1} 时概率 +{2}% ──
EffectResult effect_511_chance_power_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    const int denom = args.int_args[3];
    const int extra = args.int_args[4];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    int chance = pct;
    if (denom > 0) {
        const ElfPet& pet = ctx->getPet(self);
        const int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
        if (max_hp > 0 && pet.hp * denom < max_hp) {
            chance += extra;
        }
    }
    if (!roll_percent(ctx->rng(), chance)) {
        return EffectResult::kOk;
    }
    const int view = ctx->ws.skill_power_view[self];
    const int power = view >= 0 ? view
                                : ctx->getPet(self)
                                      .skills[ctx->roundChoice[self][1]]
                                      .power;
    ctx->ws.skill_power_view[self] = power * 2;   // 写入即置"变威力"标记（威力翻倍族）
    return EffectResult::kOk;
}

// ── 769 对手不处于异常状态 → 本次攻击伤害额外提升{0}% ──
EffectResult effect_769_no_anomaly_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (!foe_has_no_anomaly(ctx, 1 - self)) {
        return EffectResult::kOk;
    }
    ctx->grant_attack_boost(self, 769, /*count=*/1, pct);   // 本次攻击消费（SKILL_EFFECT→stage 同击）
    return EffectResult::kOk;
}

// ── 1040 当回合未击败对手 → {0}%令对手{1}；未触发 → 全属性+{2}（出手流程结束时判定）──
EffectResult encarlos_1040_at_action_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 7 || !g_core) {   // 载荷[self,foe,pad,arm_round,pct,anomaly,stat_n]
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int arm_round = args.int_args[3];
    if (self < 0 || self > 1 || arm_round != ctx->roundCount) {
        return EffectResult::kOk;   // 只在使用的当回合出手流程结束时结算
    }
    if (!mover_is(ctx, self)) {
        return EffectResult::kOk;
    }
    const int foe = 1 - self;
    if (ctx->getPet(foe).hp <= 0) {
        return EffectResult::kOk;   // 已击败：整段不结算
    }
    const int pct = args.int_args[4];
    if (roll_percent(ctx->rng(), pct) && g_core->apply_anomaly) {
        const ApplyAnomalyResult r =
            g_core->apply_anomaly(ctx, foe, args.int_args[5], /*duration=*/2, self);
        if (r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
            || r == ApplyAnomalyResult::DURATION_EXTENDED) {
            return EffectResult::kOk;   // 触发成功
        }
    }
    // 未触发补偿：全属性+{2}（含命中，749 同口径）。
    if (g_core->stat_change) {
        for (int i = 0; i < 6; ++i) {
            g_core->stat_change(ctx, self, i, args.int_args[6]);
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_1040_defer_to_action_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 5) {   // [owner, target, pct, anomaly, stat_n]
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    // 载荷 [self, foe, ?pad, arm_round, pct, anomaly, stat_n]；出手流程结束时结算。
    const std::vector<int> payload{self, 1 - self, 0, ctx->roundCount,
                                   args.int_args[2], args.int_args[3], args.int_args[4]};
    for (const State st : {State::BATTLE_FIRST_ACTION_END, State::BATTLE_SECOND_ACTION_END}) {
        Effect e(1040, 0, self, -1, EffectArgs(payload), &encarlos_1040_at_action_end);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1040;
        ce->once_ = false;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1044 吸取对手能力提升；成功 → 下{0}回合伤害翻倍（复用 776 通用增伤通道）──
EffectResult effect_1044_drain_boosts_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0 || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, 1 - self, self) <= 0) {
        return EffectResult::kOk;   // 吸取不成功 → 无后续
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1044, rounds,
                          {self, 1 - self, /*pct=*/100}, &effect_set_damage_amp);
    return EffectResult::kOk;
}

// ── 1188 {0}回合内自身受到攻击则使攻击者{1}；未触发 → 窗口末吸取对手最大体力 1/{2} ──
EffectResult encarlos_1188_expiry_check(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 6 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int expiry = args.int_args[3];
    if (self < 0 || self > 1 || ctx->roundCount != expiry) {
        return EffectResult::kOk;   // 窗口结束的那个回合开始时结算一次
    }
    auto done = ctx->plugin_storage.find(990118801 + self);
    if (done != ctx->plugin_storage.end() && any_int_or(done->second) == 1) {
        return EffectResult::kOk;   // 已触发过
    }
    ctx->plugin_storage[990118801 + self] = 2;   // 标记已结算，防重复吸取
    if (g_core->drain_hp) {
        g_core->drain_hp(ctx, self, 1 - self, args.int_args[5]);
    }
    return EffectResult::kOk;
}

EffectResult effect_1188_hit_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 5 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 受击 watcher：窗口内第一次被攻击 → 尝试给攻击者上异常；成功记 1（免疫=未触发）。
    ctx->plugin_storage[990118801 + self] = 0;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, anomaly = args.int_args[3]](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly) {
                return;
            }
            auto done = c->plugin_storage.find(990118801 + self);
            if (done == c->plugin_storage.end() || any_int_or(done->second) != 0) {
                return;   // 已触发或已结算
            }
            const int actor = ev.actor;
            if (actor < 0 || actor > 1) {
                return;
            }
            const ApplyAnomalyResult r =
                g_core->apply_anomaly(c, actor, anomaly, /*duration=*/2, self);
            if (r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
                || r == ApplyAnomalyResult::DURATION_EXTENDED) {
                c->plugin_storage[990118801 + self] = 1;
            }
        },
        WatcherScope::TEAM);
    // 窗口末检查：round_effect_start_round 起算 R..R+N-1，过期点 = 起点+N（下回合开始时）。
    const EffectWindowKind kind = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(1188)
        : EffectWindowKind::InRounds;
    const int start = ctx->round_effect_start_round(self, rounds, kind);
    const std::vector<int> payload{self, 1 - self, 0, start + rounds, 0, args.int_args[4]};
    {
        Effect e(1188, 0, self, -1, EffectArgs(payload), &encarlos_1188_expiry_check);
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_START, self,
                                                     /*duration=*/-1, ctx->roundCount);
        ce->source_id_ = 1188;
        ctx->register_skill_effect(State::BATTLE_ROUND_START, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1255 {0}回合内每回合使用技能则{1}%令对手{2}；未触发 → 附加 1/{3} 百分比伤害 ──
EffectResult encarlos_1255_round_settle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 7 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int expiry = args.int_args[3];
    if (self < 0 || self > 1 || ctx->roundCount >= expiry) {
        return EffectResult::kOk;
    }
    auto attempted = ctx->plugin_storage.find(990125501 + self);
    if (attempted == ctx->plugin_storage.end()
        || any_int_or(attempted->second) != ctx->roundCount) {
        return EffectResult::kOk;   // 本回合没用过技能
    }
    auto ok = ctx->plugin_storage.find(990125503 + self);
    if (ok != ctx->plugin_storage.end() && any_int_or(ok->second) == 1) {
        return EffectResult::kOk;   // 异常已落上
    }
    ctx->plugin_storage[990125501 + self] = -1;   // 结算完清账，防重复附加
    const ElfPet& foe = ctx->getPet(1 - self);
    if (foe.hp > 0 && g_core->deal_pink_damage) {
        const int max_hp = foe.numericalBase[NumericalPropertyIndex::HP];
        g_core->deal_pink_damage(ctx, 1 - self, max_hp / args.int_args[6],
                                 DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

EffectResult effect_1255_per_round_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 6 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const EffectWindowKind kind = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(1255)
        : EffectWindowKind::InRounds;
    const int start = ctx->round_effect_start_round(self, rounds, kind);
    ctx->plugin_storage[990125501 + self] = -1;
    // 技能使用 watcher：窗口内每回合第一次使用技能 → 掷 {1}% 上异常。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount, rounds, /*once=*/false,
        [self, start, rounds, pct = args.int_args[3], anomaly = args.int_args[4]](
            BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || !g_core || !g_core->apply_anomaly) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= start + rounds) {
                return;   // 窗口外
            }
            auto attempted = c->plugin_storage.find(990125501 + self);
            if (attempted != c->plugin_storage.end()
                && any_int_or(attempted->second) == c->roundCount) {
                return;   // 本回合已尝试
            }
            c->plugin_storage[990125501 + self] = c->roundCount;
            const ApplyAnomalyResult r =
                g_core->apply_anomaly(c, 1 - self, anomaly, /*duration=*/2, self);
            c->plugin_storage[990125503 + self] =
                (r == ApplyAnomalyResult::SUCCESS || r == ApplyAnomalyResult::REPLACED_EXISTING
                 || r == ApplyAnomalyResult::DURATION_EXTENDED)
                    ? 1
                    : 0;
        },
        WatcherScope::TEAM);
    // 每回合回合结束结算未触发补偿（窗口内）。
    const std::vector<int> payload{self, 1 - self, 0, start + rounds, 0, 0, args.int_args[5]};
    Effect e(1255, 0, self, -1, EffectArgs(payload), &encarlos_1255_round_settle);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_ROUND_END, self,
                                                 /*duration=*/-1, ctx->roundCount);
    ce->source_id_ = 1255;
    ctx->register_skill_effect(State::BATTLE_ROUND_END, self, std::move(ce));
    return EffectResult::kOk;
}

// ── 1258 造成的攻击伤害若高于{0}（严格）→ 自身免疫下{1}次受到的异常状态 ──
EffectResult encarlos_1258_at_action_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int arm_round = args.int_args[3];
    if (self < 0 || self > 1 || arm_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    if (!mover_is(ctx, self)) {
        return EffectResult::kOk;
    }
    const int dealt = ctx->ws.raw_attack_damage[self];
    if (dealt <= args.int_args[4]) {
        return EffectResult::kOk;   // 严格大于（=={0} 不触发，280 阈值族口径）
    }
    ctx->grant_immunity(self, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/99, /*source_id=*/1258,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/args.int_args[5], /*source_effect_id=*/1258);
    return EffectResult::kOk;
}

EffectResult effect_1258_high_damage_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload{self, 1 - self, 0, ctx->roundCount,
                                   args.int_args[2], args.int_args[3]};
    for (const State st : {State::BATTLE_FIRST_ACTION_END, State::BATTLE_SECOND_ACTION_END}) {
        Effect e(1258, 0, self, -1, EffectArgs(payload), &encarlos_1258_at_action_end);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/-1,
                                                     ctx->roundCount);
        ce->source_id_ = 1258;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(433, effect_433_round_stat_window);            // 无始源光：2回合每回合攻+1速+1
SKILL_EFFECT(511, effect_511_chance_power_double);          // 幻影双刃：20%威力翻倍（低体+40%）
SKILL_EFFECT(769, effect_769_no_anomaly_damage_up);         // 天河圣芒斩：对手无异常伤害+50%
SKILL_EFFECT(1040, effect_1040_defer_to_action_end);        // 光·断罪裁决：未击败80%疲惫/全属性+1
SKILL_EFFECT(1044, effect_1044_drain_boosts_double);        // 碎梦袭：吸强成功下2回合伤害翻倍
SKILL_EFFECT(1188, effect_1188_hit_anomaly_window);         // 光之惩戒：受击反制疲惫/窗口末吸取
SKILL_EFFECT(1255, effect_1255_per_round_anomaly_window);   // 光之惩戒：每回合用技失明/1/3补偿
SKILL_EFFECT(1258, effect_1258_high_damage_anomaly_immune); // 天河圣芒斩：>280 免疫下1次异常
