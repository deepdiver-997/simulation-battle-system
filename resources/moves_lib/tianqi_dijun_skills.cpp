#include <utils/sbs_rand.h>
// 天启帝君 3550 技能组（2018 年费；effect 505/821/1208/1297~1308/1315~1317）
//
// 配招（图鉴推荐）：终焉时刻 + 天君启示录 + 轮回之狱 + 天地尽灭；第五技能 纵横三千界。
// 简单技：虚空叹息(749 已有) / 流风剑道(1315+505) / 灵魂救赎(1316+1317) / 不朽圣魂(585 已有)。
//
// 官方模板（effect_info；args 按 side_effect 序逐个吃占位符，bind 后 args[0]=owner[1]=target）：
//   505  若打出致命一击，则造成伤害值的{0}%恢复自身体力          [AFTER_ACTION：读最终伤害]
//   821  {0}回合内自身的回合类效果无法被消除，后出手时回合数+1
//   1208 反转自身能力下降状态，反转成功则令对手下{0}次使用的攻击技能无效
//        [AFTER_ACTION：反转族 1221/521 同口径——本次伤害用反转前等级]
//   1297 消除对手回合类效果，消除成功则令对手{0}，未触发则吸取对手最大体力的1/{1}
//   1298 消除对手能力提升状态，消除成功则直接扣除对方场下随机一只精灵最大体力的1/4，
//        致死时令其残留1点体力
//   1299 3回合内每次对手造成的攻击伤害低于300则减少对手所有不在场精灵100点体力，
//        每次对手使用属性技能则恢复己方所有不在场精灵100点体力，
//        使对手不在场精灵死亡时令其残留1点体力
//   1300 {0}回合内受到的攻击伤害减少{1}%，受到的伤害高于{2}时随机附加{3}种异常状态，
//        未触发则消除对手回合类效果
//   1302 全属性+{0}，{1}%概率强化效果翻倍，未触发则吸取对手能力提升状态
//   1303 4回合内使用技能吸取对手最大体力1/3，自身体力低于1/2时效果翻倍，
//        吸取时若自身满体力则为己方所有不在场精灵恢复100点体力
//   1304 下{0}回合攻击忽略对手{1}%的双防值
//   1305 己方每有一只精灵存活则恢复{0}点体力，每有一只精灵死亡则附加{1}点固定伤害
//   1306 造成的伤害低于350则减少对手所有未出场精灵100点体力，使对手不在场精灵死亡时
//        令其残留1点体力            [AFTER_ACTION：1256"造成的伤害低于X"同口径读 final]
//   1307 未击败对手则恢复己方所有不在场精灵100点体力   [AFTER_ACTION：击败与否看落场后 hp]
//   1308 击败对手则自身下1次登场时帝君之罚和帝君之赐效果不会削弱 [AFTER_ACTION；写魂印侧
//        运行时（tianqi_dijun_runtime.h 共享头），下次登场由 soul_lib 罚/赐消费]
//   1315 {0}%的概率打出致命一击
//   1316 先出手时令自身随机{0}项属性+{1}
//   1317 后出手时恢复自身最大体力的1/{0}
//
// 口径备注（细节取舍见交接文档《天启帝君3550-交接.md》）：
//   · "造成的攻击伤害"阈值族（1299 的 <300 / 1300 的 >280）取**裸伤台账**
//     ws.raw_attack_damage（1263 ④ 同款——"造成的攻击伤害"不看减伤/挡伤）；两条同源，
//     官方在 280~300 留了死区，取同一值域才自洽。
//   · 场下扣血的伤害类型按官方措辞分两族（用户 2026-09-20 口径）：
//     "减少X点体力"（1299/1306）= **真实伤害**（不因抗性减免）→ 走
//     deal_off_field_true_damage 原语（residue_floor=1 表达"致死时残留1点"）；
//     "直接扣除"（罚/1298）= 直接扣减（非伤害、无类型）→ 直写 hp + 武心婵保护门 + 钳底 1。
//     场下加血：直写钳上限。
//   · 821/1299 的"回合数"走官方 InRounds；1304 的"下N回合"走 NextRounds。
//   · 1315 用"独立掷点 → 本次必暴"表达（会心 1~5 星的独立二次结算口径）。
#include "common.h"

#include "../tianqi_dijun_runtime.h"

namespace {

// ── 场下遍历助手（本文件多处用；与 soul_lib 各自本地持有，跨 .so 不共享）──
// 对 side 方所有**不在场存活**精灵逐个调 fn(slot, pet)；skip_protected=true 时
// 先过武心婵保护门（扣血路径必过，恢复路径不查——保护门只挡"减少"）。
template <typename Fn>
inline void for_each_off_field_alive(BattleContext* ctx, int side, bool skip_protected, Fn fn) {
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[side]) {
            continue;   // 不在场限定
        }
        ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
        if (pet.hp <= 0 || ctx->is_vanished(side, slot)) {
            continue;
        }
        if (skip_protected && ctx->off_field_stats_protected(side, slot)) {
            continue;
        }
        fn(slot, pet);
    }
}

// 场下精灵的最大体力（与 soul_lib 的 resolve_pet_max_hp 同义；跨 .so 不共享，本地一份）。
inline int pet_max_hp(const ElfPet& pet) {
    int max_hp = pet.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = pet.numericalProperties[NumericalPropertyIndex::HP];
    }
    if (max_hp <= 0) {
        max_hp = pet.hp;
    }
    return max_hp;
}

// "先/后出手"判定：读**先手权解析结果**（MOVE_RIGHT 时点定的 preemptive_right），
// 不读 currentState 桶位——对手嗑药/被跳过时，唯一行动方的动作会被提升进 FIRST 槽，
// 桶位判"先后手"会静默翻转（J2 实测踩坑）。NONE（双方都没解析出先手）→ 两分支都不触发。
inline bool is_first_mover(BattleContext* ctx, int self) {
    if (!ctx || self < 0 || self > 1) {
        return false;
    }
    switch (ctx->preemptive_right) {
        case PreemptiveRight::SEER_ROBOT_1: return self == 0;
        case PreemptiveRight::SEER_ROBOT_2: return self == 1;
        default: return false;
    }
}

// 存活/阵亡计数（1305 用；剔消逝——消逝既不算存活也不算阵亡）。
inline void count_alive_dead(BattleContext* ctx, int side, int& alive, int& dead) {
    alive = 0;
    dead = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->is_vanished(side, slot)) {
            continue;
        }
        ctx->seerRobot[side].elfPets[slot].hp > 0 ? ++alive : ++dead;
    }
}

// ── 1297 消回合 → 害怕 / 未触发 → 吸 1/{denom} ──────────────────────────
EffectResult effect_1297_break_then_branch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, target) == BreakResult::SUCCESS) {
        if (g_core->apply_anomaly) {
            g_core->apply_anomaly(ctx, target, anomaly, /*duration=*/2, self);
        }
    } else if (g_core->drain_hp) {
        g_core->drain_hp(ctx, self, target, denom);   // "未触发则吸取"
    }
    return EffectResult::kOk;
}

// ── 1298 消强 → 扣对方场下随机一只 1/4（致死留 1）────────────────────────
EffectResult effect_1298_clear_boost_punish_off_field(BattleContext* ctx,
                                                      const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 消除不成功 → 无后续
    }
    std::vector<int> candidates;
    for_each_off_field_alive(ctx, target, /*skip_protected=*/true,
                             [&candidates](int slot, const ElfPet&) {
                                 candidates.push_back(slot);
                             });
    if (candidates.empty()) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->seerRobot[target].elfPets[candidates[sbs_rand() % candidates.size()]];
    const int dmg = pet_max_hp(pet) / 4;   // 模板明写 1/4（无占位符）
    if (dmg > 0) {
        pet.hp = std::max(1, pet.hp - dmg);   // "致死时令其残留1点体力"
    }
    return EffectResult::kOk;
}

// ── 1299 轮回之狱·场下调剂半边（3 回合域，数值全明写无占位符）──────────────
// ① watcher：对手攻击裸伤 <300 → 对手所有不在场 -100（钳底 1）
// ② ROUND_END tick：对手本回合用了属性技能 → 己方所有不在场 +100
constexpr int kKey1299Window = 1299000;   // plugin_storage 键：+self

EffectResult effect_1299_attr_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    // 对手本回合是否"真的用了属性技能"（被控时引擎把 roundChoice 清成 NONE）
    if (ctx->roundChoice[target][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[target][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(target).skills.size())
        || ctx->getPet(target).skills[static_cast<std::size_t>(idx)].type
               != SkillType::Attribute) {
        return EffectResult::kOk;
    }
    for_each_off_field_alive(ctx, self, /*skip_protected=*/false, [](int, ElfPet& pet) {
        pet.hp = std::min(pet_max_hp(pet), pet.hp + 100);   // 明写 100 点
    });
    return EffectResult::kOk;
}

EffectResult effect_1299_prison_domain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 窗口（官方"3回合内"）：InRounds 起点，存 plugin_storage 供 watcher 读；
    // 重复使用只刷新窗口（408 套路），watcher 条目不重复注册。
    using Win = std::pair<int, int>;   // [start, expire)
    const int start = ctx->round_effect_start_round(self, 3, EffectWindowKind::InRounds);
    auto win = std::make_shared<Win>(start, start + 3);
    if (!ctx->plugin_storage.count(kKey1299Window + self)) {
        ctx->plugin_storage[kKey1299Window + self] = win;
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [self, target, win](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != target || ev.target != self) {
                    return;   // 只认"对手打到己方"的攻击伤害
                }
                if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                    && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                    return;
                }
                const int rc = wctx->roundCount;
                if (rc < win->first || rc >= win->second) {
                    return;   // 窗口外
                }
                if (wctx->ws.raw_attack_damage[target] >= 300) {
                    return;   // 裸伤台账（口径见文件头）
                }
                if (!g_core || !g_core->deal_off_field_true_damage) {
                    return;
                }
                // 与 1306 同款措辞"减少…体力" = **真实伤害**（用户 2026-09-20 口径）→
                // 走对场下真伤原语，residue_floor=1（"致死时残留1点"）
                for (int slot = 0; slot < 6; ++slot) {
                    if (slot == wctx->on_stage[target]) {
                        continue;   // 不在场限定
                    }
                    g_core->deal_off_field_true_damage(wctx, target, slot, /*amount=*/100,
                                                       self, /*residue_floor=*/1);
                }
            },
            WatcherScope::TEAM);
    } else {
        *std::any_cast<std::shared_ptr<Win>>(ctx->plugin_storage[kKey1299Window + self]) = *win;
    }
    // 属性技回血 tick：窗口管理交给 ContinuousEffect（register_round_effect 同源去重）
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 1299, 3,
                          {self, target}, &effect_1299_attr_heal_tick);
    return EffectResult::kOk;
}

// ── 1300 轮回之狱·受伤减半 + 阈值异常半边（{0}=3回合 {1}=50 {2}=280 {3}=2）────
constexpr int kKey1300Window = 1300000;

EffectResult effect_1300_prison_guard(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int reduce_pct = args.int_args[3];
    const int threshold = args.int_args[4];
    const int anomaly_count = args.int_args[5];
    if (self < 0 || self > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    using Win = std::pair<int, int>;
    const int start =
        ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    auto win = std::make_shared<Win>(start, start + rounds);
    if (!ctx->plugin_storage.count(kKey1300Window + self)) {
        ctx->plugin_storage[kKey1300Window + self] = win;
        // (a) 受到攻击伤害 -{1}%：REDUCE_PCT 阶段回调（525 窗口判套路）
        ctx->register_damage_effect(
            DamagePhase::REDUCE_PCT,
            self,
            DamageEffectCategory::MITIGATE,
            [self, reduce_pct, win](BattleContext* c, int owner) {
                if (!c || owner != self) {
                    return;
                }
                const int rc = c->roundCount;
                if (rc < win->first || rc >= win->second) {
                    return;
                }
                DamageSnapshot& damage = c->resolvedDamage;
                if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                    return;   // 只减"受到的攻击伤害"（红伤）
                }
                damage.final = damage.final * (100 - reduce_pct) / 100;
            });
        // (b) 裸伤 >{2} → 随机 {3} 种异常；一个都没进 → 消对手回合类效果
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [self, target, threshold, anomaly_count, win](BattleContext* wctx,
                                                          const BattleEvent& ev) {
                if (!wctx || ev.actor != target || ev.target != self || !g_core) {
                    return;
                }
                if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                    && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                    return;
                }
                const int rc = wctx->roundCount;
                if (rc < win->first || rc >= win->second) {
                    return;
                }
                if (wctx->ws.raw_attack_damage[target] <= threshold) {
                    return;
                }
                if (g_core->attach_random_anomalies) {
                    const RandomAnomalyResult r = g_core->attach_random_anomalies(
                        wctx, target, anomaly_pool_all(), anomaly_count, /*probability_pct=*/100,
                        self, ChanceSource::Skill);
                    if (r.landed == 0 && g_core->break_round_effects) {
                        g_core->break_round_effects(wctx, target);   // "未触发则消除"
                    }
                }
            },
            WatcherScope::TEAM);
    } else {
        *std::any_cast<std::shared_ptr<Win>>(ctx->plugin_storage[kKey1300Window + self]) = *win;
    }
    return EffectResult::kOk;
}

// ── 821 {0}回合内回合类效果免消除，后出手时回合数+1 ──────────────────────
EffectResult effect_821_break_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    const int extra = is_first_mover(ctx, self) ? 0 : 1;   // "后出手时回合数+1"
    ctx->grant_immunity(self, ImmunityType::BREAK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds + extra, /*source_id=*/821);
    return EffectResult::kOk;
}

// ── 1302 全属性+{0}，{1}%翻倍，未触发吸强 ────────────────────────────────
EffectResult effect_1302_all_up_maybe_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int boost = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || target < 0 || target > 1 || boost == 0) {
        return EffectResult::kOk;
    }
    const bool doubled = roll_percent(pct);
    all_stats_up(ctx, self, doubled ? boost * 2 : boost);
    if (!doubled && g_core && g_core->transfer_stat_boosts) {
        g_core->transfer_stat_boosts(ctx, /*from=*/target, /*to=*/self);   // "未触发则吸取"
    }
    return EffectResult::kOk;
}

// ── 1303 4回合内用技能吸取对手最大体力1/3；自身体力<1/2 翻倍；满体力转为己方场下 +100
// （翻倍 = 两次 drain_hp：两段独立结算——次数型免粉各挡一段，实测若要求单段双值再改）
EffectResult effect_1303_round_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->drain_hp) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self) || denom <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const bool was_full = ctx->getPet(self).hp >= max_hp;
    const bool low = ctx->getPet(self).hp < max_hp / 2;
    g_core->drain_hp(ctx, /*actor=*/self, /*target=*/target, denom);
    if (low) {
        g_core->drain_hp(ctx, self, target, denom);   // "效果翻倍"
    }
    if (was_full) {
        // "吸取时若自身满体力则"：自身回不动（钳满）→ 转给己方所有不在场存活精灵
        for_each_off_field_alive(ctx, self, /*skip_protected=*/false, [](int, ElfPet& pet) {
            pet.hp = std::min(pet_max_hp(pet), pet.hp + 100);
        });
    }
    return EffectResult::kOk;
}

EffectResult effect_1303_round_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // "4回合内"明写（无占位符），denom 明写 1/3
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1303, 4, {self, target, 3},
                              &effect_1303_round_drain_tick);
    }
    return EffectResult::kOk;
}

// ── 1304 下{0}回合攻击忽略对手{1}%的双防值 ───────────────────────────────
// 无现成"忽略防御"通道 → 窗口内每回合把 ws.battle_attrs[对手] 双防 ×(100-{1})%：
// sync（回合开始/换宠）先于 ROUND_START 桶重基 battle_attrs → tick 每回合对**新鲜值**
// 乘一次，不叠加；对手换宠会重基（本回合余下时间失去削减，边界见交接文档）。
EffectResult effect_1304_ignore_def_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    auto scale = [&](NumericalPropertyIndex idx) {
        ctx->ws.battle_attrs[target][idx] =
            ctx->ws.battle_attrs[target][idx] * (100 - pct) / 100;
    };
    scale(NumericalPropertyIndex::DEFENSE);
    scale(NumericalPropertyIndex::SPECIAL_DEFENSE);
    return EffectResult::kOk;
}

EffectResult effect_1304_ignore_def(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || target < 0 || target > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1304, rounds,
                                {self, target, pct}, &effect_1304_ignore_def_tick);
    return EffectResult::kOk;
}

// ── 1305 己方每有一只存活 → 回{0}点；每有一只阵亡 → 附加{1}点固定伤害 ──────
EffectResult effect_1305_alive_heal_dead_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int per_alive = args.int_args[2];
    const int per_dead = args.int_args[3];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    int alive = 0;
    int dead = 0;
    count_alive_dead(ctx, self, alive, dead);
    if (alive > 0 && per_alive > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, self, alive * per_alive);
    }
    if (dead > 0 && per_dead > 0 && g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, dead * per_dead);
    }
    return EffectResult::kOk;
}

// ── 1306 造成的伤害低于350 → 对手所有未出场 -100（致死留 1）[AFTER_ACTION] ──
// ★ 伤害类型 = **真实伤害**（用户 2026-09-20 口径：不因抗性减免；与"对场下造成粉伤"
//   的效果族区分开）→ 走专门的对场下真伤原语（residue_floor=1 表达"致死时残留1点"），
//   不再直写 hp。保护门/存活过滤/在场拒绝都在原语内。
EffectResult effect_1306_low_damage_punish(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.final >= 350) {
        return EffectResult::kOk;   // 1256 口径：读管线后最终伤害
    }
    if (!g_core || !g_core->deal_off_field_true_damage) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[target]) {
            continue;   // 不在场限定
        }
        g_core->deal_off_field_true_damage(ctx, target, slot, /*amount=*/100, self,
                                           /*residue_floor=*/1);
    }
    return EffectResult::kOk;
}

// ── 1307 未击败对手 → 己方所有不在场 +100 [AFTER_ACTION] ─────────────────
EffectResult effect_1307_no_ko_heal_off_field(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->seerRobot[target].elfPets[ctx->on_stage[target]].hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 不恢复
    }
    for_each_off_field_alive(ctx, self, /*skip_protected=*/false, [](int, ElfPet& pet) {
        pet.hp = std::min(pet_max_hp(pet), pet.hp + 100);
    });
    return EffectResult::kOk;
}

// ── 1308 击败对手 → 下1次登场罚/赐不削弱 [AFTER_ACTION；写共享运行时] ──────
EffectResult effect_1308_ko_no_decay_flag(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->seerRobot[target].elfPets[ctx->on_stage[target]].hp > 0) {
        return EffectResult::kOk;   // 未击败
    }
    dijun_rt(ctx->getPet(self).soulmark_storage).next_entry_no_decay = true;
    return EffectResult::kOk;
}

// ── 505 打出致命一击 → 伤害值{0}%回血 [AFTER_ACTION] ─────────────────────
EffectResult effect_505_crit_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const DamageSnapshot& damage = ctx->resolvedDamage;
    if (damage.attackerId != self || !damage.isCrit || damage.final <= 0) {
        return EffectResult::kOk;
    }
    g_core->heal_amount(ctx, self, damage.final * pct / 100);
    return EffectResult::kOk;
}

// ── 1208 反转自身降级 → 成功则对手下{0}次攻击技能无效 [AFTER_ACTION] ───────
EffectResult effect_1208_reversal_then_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->stat_reversal || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // "反转成功则"——NOTHING/BLOCKED 都不算
    }
    g_core->seal_skill(ctx, /*source=*/self, /*target=*/target, /*effect_id=*/1208,
                       /*attribute=*/false, /*attack=*/true,
                       /*count=*/count, /*duration_rounds=*/0, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/true,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 1315 {0}%的概率打出致命一击（本次攻击；独立掷点 → 必暴）────────────────
EffectResult effect_1315_chance_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (roll_percent(pct)) {
        ctx->ws.must_crit[self] = true;   // SKILL_EFFECT 时点写 → 本次 ATTACK_DAMAGE 消费
    }
    return EffectResult::kOk;
}

// ── 1316 先出手时令自身随机{0}项属性+{1} ────────────────────────────────
EffectResult effect_1316_first_random_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int count = args.int_args[2];
    const int delta = args.int_args[3];
    if (self < 0 || self > 1 || count <= 0 || delta == 0) {
        return EffectResult::kOk;
    }
    if (!is_first_mover(ctx, self)) {
        return EffectResult::kOk;   // "先出手时"
    }
    // 从 6 个能力槽（0攻/1特攻/2防/3特防/4速/5命中）不重复抽 count 项（部分 Fisher-Yates）
    int slots[BattleContext::kAbilityLevelSlotCount];
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        slots[i] = i;
    }
    const int picks = std::min(count, BattleContext::kAbilityLevelSlotCount);
    for (int i = 0; i < picks; ++i) {
        const int j = i + sbs_rand() % (BattleContext::kAbilityLevelSlotCount - i);
        const int tmp = slots[i];
        slots[i] = slots[j];
        slots[j] = tmp;
        g_core->stat_change(ctx, self, slots[i], delta);
    }
    return EffectResult::kOk;
}

// ── 1317 后出手时恢复自身最大体力的1/{0} ────────────────────────────────
EffectResult effect_1317_second_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (is_first_mover(ctx, self)) {
        return EffectResult::kOk;   // "后出手时"
    }
    heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
SKILL_EFFECT(505, effect_505_crit_heal);                    // 流风剑道：暴击回血50%
SKILL_EFFECT(821, effect_821_break_immune);                 // 轮回之狱：回合类免消除
SKILL_EFFECT(1208, effect_1208_reversal_then_seal);         // 天君启示录：反转→封攻击
SKILL_EFFECT(1297, effect_1297_break_then_branch);          // 天地尽灭：消回合二分支
SKILL_EFFECT(1298, effect_1298_clear_boost_punish_off_field);  // 天地尽灭：消强扣场下
SKILL_EFFECT(1299, effect_1299_prison_domain);              // 轮回之狱：场下调剂域
SKILL_EFFECT(1300, effect_1300_prison_guard);               // 轮回之狱：减伤+阈值异常域
SKILL_EFFECT(1302, effect_1302_all_up_maybe_double);        // 终焉时刻：全属性+翻倍/吸强
SKILL_EFFECT(1303, effect_1303_round_drain);                // 终焉时刻：吸取域
SKILL_EFFECT(1304, effect_1304_ignore_def);                 // 终焉时刻：忽略双防
SKILL_EFFECT(1305, effect_1305_alive_heal_dead_fixed);      // 天君启示录：存活/阵亡账
SKILL_EFFECT(1306, effect_1306_low_damage_punish);          // 纵横三千界：低伤扣场下
SKILL_EFFECT(1307, effect_1307_no_ko_heal_off_field);       // 纵横三千界：未击败回场下
SKILL_EFFECT(1308, effect_1308_ko_no_decay_flag);           // 纵横三千界：击败→罚赐不削弱
SKILL_EFFECT(1315, effect_1315_chance_crit);                // 流风剑道：50%暴击
SKILL_EFFECT(1316, effect_1316_first_random_up);            // 灵魂救赎：先手随机3项+1
SKILL_EFFECT(1317, effect_1317_second_heal);                // 灵魂救赎：后手回1/4
