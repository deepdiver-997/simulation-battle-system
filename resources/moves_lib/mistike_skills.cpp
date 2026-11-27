#include <utils/sbs_rand.h>
#include <cstdio>
// 神觉·米斯蒂克 4676 技能组（官方参数 moves.side_effect / effect_info，2026-09-20 查库）：
//   溃梦灵言 37509：1471 消强成功→疲惫，未触发→回1/3 ｜ 2212 受击疲惫控+挡伤 ｜ 776 攻伤翻倍(已实现)
//   心朽魂凋 28808：1848 对手有下降→先制+2 ｜ 191 免疫反弹异常(已实现) ｜ 47 免弱(已实现) ｜
//                   449 对手有下降→100%疲惫 ｜ 1684 对手下降不可解除/反转 ｜
//                   1428 100%全属性-1 + 3回合每回合攻/特攻/速-1
//   魂劫骇咒 28809：1338 全属性+1，对手有下降翻倍 ｜ 2076 4回合每技吸取1/3 ｜
//                   843 先制+1(已实现) ｜ 2211 附加蚩庸之锁
//   堕之盛响 37510：889 消回合成功→疲惫(已实现) ｜ 2213 自爆传承盾罩 ｜ 2211 +3层锁 ｜
//                   2148 己方免疫下1次异常（TEAM 传承）
//   王·御影神罚 37511：760 不微弱(已实现) ｜ 1114 消回合成功→随机2项PP归零 ｜
//                   907 反转下降成功→随机2项PP归零 ｜ 2214 每层锁→随机1攻击技PP归零 ｜
//                   1827 击败→对手下1次属性技能无效
//
// 2212 拆分口径（用户 2026-09-20 拍板）：
//   ★ **受击疲惫控不会被断回合打断**——窗口记在 plugin_storage（不可断），只在 3 回合自然到期；
//   ★ **挡伤**（未触发疲惫→抵挡当次攻击伤害，技能效果照常）：随窗口的**挡伤哨兵**走
//     （回合类 ContinuousEffect，可被断回合清掉）；且**对面有魂免/次免把疲惫挡下时不算
//     "未触发"**——不挡伤；
//   ★ 施加疲惫**弹控反弹也算成功**（弹回自己身上落了疲惫就成功）；只有"最终没人被疲惫"
//     （如自己也有免疫导致弹回也失败）才算失败 → 挡伤。
//   ★ 疲惫在对手技能效果和伤害结算之前（EVENT_HIT/EVENT_SKILL_INVALID 在 ON_SKILL_HIT
//     桶后 drain——天然早于其 SKILL_EFFECT 桶，也早于"消回合补偿次免"的解析口径）。
#include "common.h"
#include "../mistike_runtime.h"

namespace {

// ── 蚩庸之锁读写（moves_lib 侧；印记在 pet.marks 上，跨 dylib 天然共享）────────
int lock_count_of(const ElfPet& pet) {
    for (const Mark& mark : pet.marks) {
        if (mark.name == mistike_lock_name()) {
            return mark.count;
        }
    }
    return 0;
}

int opp_lock_count(BattleContext* ctx, int owner) {
    return lock_count_of(ctx->getPet(1 - owner));
}

void set_lock_count(BattleContext* ctx, int opp, int n) {
    mistike_publish_lock_slot(ctx->plugin_storage, opp, n > 0 ? ctx->on_stage[opp] : -1);
    ElfPet& pet = ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]];
    if (n <= 0) {
        std::erase_if(pet.marks, [](const Mark& mark) {
            return mark.name == mistike_lock_name();
        });
    } else {
        for (Mark& mark : pet.marks) {
            if (mark.name == mistike_lock_name()) {
                mark.count = n;
                mistike_publish_locks(ctx->plugin_storage, opp, n);
                return;
            }
        }
        pet.marks.emplace_back(0, mistike_lock_name(), n);   // Mark(id, name, count)
    }
    mistike_publish_locks(ctx->plugin_storage, opp, std::max(0, n));
}

bool side_has_stat_drop(BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] < 0) {
            return true;
        }
    }
    return false;
}

// ── 1848 —— 对手处于能力下降状态时先制+2（选择期；loadSkills 已把它路由进 selection_effects_，
//             与 2000/610 同路——先手权比较前生效，SKILL_EFFECT 时点注册就太晚了）──
EffectResult effect_1848_drop_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (side_has_stat_drop(ctx, opp)) {
        ctx->ws.preemptive_level[owner] += 2;
    }
    return EffectResult::kOk;
}

// ── 449 —— 若对手处于能力下降状态则{0}%概率{1} ──
// Args=[owner, target, chance, anomaly]
EffectResult effect_449_conditional_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (!side_has_stat_drop(ctx, opp) || !roll_percent(ctx->rng(), chance)) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 1684 —— {0}回合内对手的能力下降状态无法被解除或反转 ──
// 落点 = 新 ImmunityType::STAT_DROP_LOCK（内核 clear_stat_drops 返 0 / stat_reversal 返 BLOCKED）。
// 票挂**对手**（护的是"它的下降不被拿走"）；等级本体 on-stage 作用域（换宠即清）→ ON_STAGE。
EffectResult effect_1684_lock_drops(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(opp, ImmunityType::STAT_DROP_LOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1684,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1684);
    return EffectResult::kOk;
}

// ── 1428 —— {0}%使对手全属性-1，{1}回合内每回合对手攻击/特攻/速度-1 ──
// Args=[owner, target, chance, rounds]（库中后续 6 参为 (档,量) 对；图鉴/B站解析口径=
// 攻击-1、特攻-1、速度-1，写死）。"每回合"半边 = 回合类（可被断回合），走
// register_round_effect 的 InRounds 折算窗口（先出手=当回合起算——本回合 ROUND_START 桶
// 已跑过，自然从下回合开始落）。
constexpr int k1428RoundlyDropSlots[3] = {0, 1, 4};   // 攻击 / 特攻 / 速度

EffectResult effect_1428_roundly_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    for (const int stat : k1428RoundlyDropSlots) {
        g_core->stat_drop(ctx, opp, stat, 1, owner);
    }
    return EffectResult::kOk;
}

EffectResult effect_1428_all_down_then_roundly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (!roll_percent(ctx->rng(), chance)) {
        return EffectResult::kOk;
    }
    // 即时半边：全属性-1（含命中，kAbilityLevelSlotCount 口径；逐项查免弱、钳 -6）。
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        g_core->stat_drop(ctx, opp, i, 1, owner);
    }
    // 每回合半边：回合类，注册到 ROUND_START 桶。
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 1428, rounds,
                          {owner, opp}, &effect_1428_roundly_drop);
    return EffectResult::kOk;
}

// ── 1338 —— 全属性+{0}，对手处于能力下降状态时强化效果翻倍 ──
EffectResult effect_1338_all_up_maybe_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int boost = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || boost <= 0) {
        return EffectResult::kOk;
    }
    const int mult = side_has_stat_drop(ctx, opp) ? 2 : 1;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        g_core->stat_change(ctx, owner, i, boost * mult);
    }
    return EffectResult::kOk;
}

// ── 2076 —— {0}回合内使用技能吸取对手最大体力的1/{1}，自身体力低于1/{2}时效果翻倍，
//             吸取后若对手体力未减少则为己方所有场下存活精灵恢复{3}点体力 ──
// Args=[owner, target, rounds, denom, low_denom, fallback_heal]
// "使用技能吸取" = 窗口内她**每次**技能（攻击/属性都算）：挂她自己的 FIRST/SECOND
// SKILL_EFFECT 桶（桶只跑 mover 侧 → 天然只在她出手时结算；窗口到期由桶清理）。
EffectResult effect_2076_drain_tick_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int victim = args.int_args[1];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    const int team_heal = args.int_args[5];
    if (self < 0 || self > 1 || victim < 0 || victim > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& victim_pet = ctx->seerRobot[victim].elfPets[ctx->on_stage[victim]];
    if (victim_pet.hp <= 0) {
        return EffectResult::kOk;
    }
    int amount = max_hp_of(ctx, victim) / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp < max_hp_of(ctx, self) / low_denom) {
        amount *= 2;   // 自身体力低于 1/{2} → 效果翻倍
    }
    const int before = victim_pet.hp;
    g_core->fixed_damage(ctx, victim, amount);
    const int dealt = before - victim_pet.hp;
    if (dealt > 0) {
        if (g_core->heal_amount) {
            g_core->heal_amount(ctx, self, dealt);   // 吸取 = 回自身（吃封回血/恢复修正）
        }
        return EffectResult::kOk;
    }
    // 对手体力未减少（被护罩全吃等）→ 己方所有**场下**存活精灵恢复 {3} 点
    // （场下精灵不在结算视图里，直写 hp、钳到各自上限——defeat 同步由 sync_pending_deaths 兜底，
    // 这里只加血不会触发死亡）。
    if (team_heal <= 0) {
        return EffectResult::kOk;
    }
    for (int slot = 0; slot < static_cast<int>(ctx->seerRobot[self].elfPets.size()); ++slot) {
        if (slot == ctx->on_stage[self]) {
            continue;
        }
        ElfPet& mate = ctx->seerRobot[self].elfPets[slot];
        if (mate.hp <= 0) {
            continue;   // 存活才回
        }
        int cap = mate.numericalBase[NumericalPropertyIndex::HP];
        if (cap <= 0) {
            cap = mate.hp;
        }
        mate.hp = std::min(cap, mate.hp + team_heal);
    }
    return EffectResult::kOk;
}

EffectResult effect_2076_drain_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, 2076, rounds,
                          {args.int_args[0], args.int_args[1], args.int_args[2],
                           args.int_args[3], args.int_args[4], args.int_args[5]},
                          &effect_2076_drain_tick_fixed);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, 2076, rounds,
                          {args.int_args[0], args.int_args[1], args.int_args[2],
                           args.int_args[3], args.int_args[4], args.int_args[5]},
                          &effect_2076_drain_tick_fixed);
    return EffectResult::kOk;
}

// ── 1471 —— 消除对手能力提升状态，消除成功则{0}%令对手{1}，未触发则恢复自身最大体力的1/{2} ──
// Args=[owner, target, chance, anomaly, denom]。"未触发"含消强未成功（936 同口径：补偿看
// 的是"异常最终没落地"）。
EffectResult effect_1471_clear_then_fatigue(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    bool landed = false;
    if (g_core->clear_stat_boosts && g_core->clear_stat_boosts(ctx, opp) > 0
        && roll_percent(ctx->rng(), chance)) {
        const ApplyAnomalyResult r =
            g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), owner);
        landed = (r == ApplyAnomalyResult::SUCCESS
                  || r == ApplyAnomalyResult::REPLACED_EXISTING
                  || r == ApplyAnomalyResult::DURATION_EXTENDED
                  || r == ApplyAnomalyResult::CONVERTED);
    }
    if (!landed && denom > 0) {
        heal_max_hp_fraction(ctx, owner, denom);
    }
    return EffectResult::kOk;
}

// ── 2148 —— 己方免疫下{0}次受到的异常状态 ──
// 己方 = owner 一侧整队：TEAM 域 + 次数型票（传承——堕之盛响自爆后由上场队友继承）。
EffectResult effect_2148_side_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/2148,
                        /*soul_immunity=*/false, EffectScope::TEAM,
                        /*counts=*/count, /*source_effect_id=*/2148);
    return EffectResult::kOk;
}

// ── 2211 —— 为对手附加{0}层蚩庸之锁，若超出了上限则对手下{1}回合先制-{2} ──
// Args=[owner, target, n, rounds, prio_down]。上限 3（魂印与技能共用同一套层，用户确认）；
// 溢出部分 → "下{1}回合先制-{2}"（NextRounds 窗口，回合类 MOVE_RIGHT 桶）。
EffectResult effect_2211_add_locks(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int n = args.int_args[2];
    const int rounds = args.int_args[3];
    const int prio_down = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    const int cur = opp_lock_count(ctx, owner);
    const int add = std::min(n, kMistikeLockCap - cur);
    if (add > 0) {
        set_lock_count(ctx, opp, cur + add);
    }
    const int overflow = n - add;
    if (overflow > 0 && rounds > 0 && prio_down != 0) {
        // 超上限 → 对手下{1}回合先制-{2}（回合类，走 NextRounds：下回合起算）。
        register_next_rounds_effect(
            ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 2211, rounds,
            {owner, opp, -prio_down},
            [](BattleContext* c, const EffectArgs& a) -> EffectResult {
                if (!c || !a.int_args || a.int_count < 3) {
                    return EffectResult::kOk;
                }
                const int victim = a.int_args[1];
                const int delta = a.int_args[2];
                if (victim < 0 || victim > 1) {
                    return EffectResult::kOk;
                }
                c->ws.preemptive_level[victim] += delta;   // delta 为负
                return EffectResult::kOk;
            });
    }
    return EffectResult::kOk;
}

// ── 2213 —— 消耗自身全部体力，己方下只精灵获得点数等同于消耗值{0}%的护盾与护罩 ──
// Args=[owner, target, pct]。随技能附赠"N回合内对手属性技能无效"（自爆包一部分）。
//
// ★ **自爆完全生效 + 防同归于尽在死亡检测点拦截**（用户 2026-09-20 终版口径）：
//   官方时序 = 对手先被本次攻击打死、自爆降体力在后 → 降体力结算挂本方 AFTER_ACTION
//   （伤害结算之后）。结算时传承照常挂起（自爆效果不打折扣）；**死亡检测同时看对手**：
//   对手已死 → 本次不登记死亡、不发 EVENT_DEATH 广播，直接强制留 1 点体力；
//   对手还活着才走 defeat_pet(HP_CONSUME) 死亡漏斗。
// 传承 = plugin_storage 挂起值，己方下一次 EVENT_ENTER_STAGE 落盾+罩（ShieldBank 直写，
// source_id=2213、破盾前持续）。
constexpr int k2213PendingKey = 2213900;   // +owner：挂起的传承值（int）
constexpr int k2213WatcherKey = 2213901;   // +owner：监听器已挂标记
constexpr int k2213SealRounds = 3;         // 属性技能无效回合数（官方文本 3 回合，用户 2026-09-20 确认）

// 降体力结算体（挂在施放方的 AFTER_ACTION 桶——桶只跑 mover 侧，once 一次性）。
EffectResult effect_2213_resolve(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->defeat_pet) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int consumed = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || consumed <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 结算前已死（反弹/反伤等）——自爆无从谈起，不动
    }
    // ① 传承照常挂起（自爆效果不打折扣，防同归只救命）。
    ctx->plugin_storage[k2213PendingKey + owner] = consumed * pct / 100;

    // ② 死亡检测同时看对手（用户终版口径）：对手已死（被本次攻击打死）→ 本次**不登记
    //    死亡、不发 EVENT_DEATH 广播**，直接强制留 1 点体力；对手还活着才走死亡漏斗
    //    （defeat_pet 契约 = 调用方先把体力打到 0；HP_CONSUME 不响应免疫秒杀、穿残留免死）。
    if (ctx->seerRobot[opp].elfPets[ctx->on_stage[opp]].hp <= 0) {
        self.hp = 1;
        return EffectResult::kOk;
    }
    self.hp = 0;
    g_core->defeat_pet(ctx, owner, slot, /*actor=*/opp, DefeatCause::HP_CONSUME);

    // 己方下一次上场（换宠）落盾+罩（监听器只挂一次）。
    if (!ctx->plugin_storage.count(k2213WatcherKey + owner)) {
        ctx->plugin_storage.emplace(k2213WatcherKey + owner, 1);
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [owner](BattleContext* c, const BattleEvent& ev) {
                auto pend = c->plugin_storage.find(k2213PendingKey + owner);
                if (pend == c->plugin_storage.end()) {
                    return;
                }
                const int value = any_int_or(pend->second);
                c->plugin_storage.erase(pend);
                if (value <= 0 || ev.actor != owner) {
                    return;
                }
                ElfPet& pet = c->seerRobot[owner].elfPets[c->on_stage[owner]];
                constexpr int kSource = 2213;
                pet.shield_bank_.add(/*priority=*/0, value, kSource, /*duration=*/0);
                pet.hood_bank_.add(/*priority=*/0, value, kSource, /*duration=*/0);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

EffectResult effect_2213_self_destruct(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->defeat_pet) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    ElfPet& self = ctx->seerRobot[owner].elfPets[slot];
    if (self.hp <= 0) {
        return EffectResult::kOk;
    }
    // 随技能附赠的封属：一旦使用出来就生效（TEAM 域——自爆死亡后封属仍在，随回合 tick 递减）。
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/args.int_args[1],
                       /*effect_id=*/2213, /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/k2213SealRounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::TEAM, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    // 消耗值在技能结算时点捕获（AFTER_ACTION 结算时用）；降体力本体推迟到
    // 本方 AFTER_ACTION（伤害结算之后）——见 effect_2213_resolve 的防同归于尽口径。
    const int consumed = self.hp;
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(2213, 0, owner, /*left_round=*/1,
                 EffectArgs(std::vector<int>{owner, args.int_args[1], consumed, pct}),
                 &effect_2213_resolve);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 2213;
        ce->once_ = true;   // 两个状态各挂一条；只有真正行动的那条会执行，另一条回合末过期
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 2212 —— {0}回合内对手使用威力不高于{1}的攻击技能时令对手{2}，未触发则抵挡对手当次攻击 ──
// Args=[owner, target, rounds, threshold, anomaly]
//
// 结构（拆分口径见文件头）：
//   · 疲惫窗口（**不可断**）：plugin_storage 记 (start, rounds)，start 走
//     round_effect_start_round 的 InRounds 折算（先出手=当回合起 3 回合）。
//   · 挡伤哨兵（**可断**）：回合类 ContinuousEffect（no-op，只当"挡伤半边还活着"的
//     存在性凭证，hasEffect 查询；断回合清掉 = 挡伤半边失效）。
//   · 挡伤执行：BLOCK 阶段常驻条目（按 skill_use_seq 对准被挡的那一击），零化红伤、
//     技能效果照常（效果已在 SKILL_EFFECT 桶注册过）。
//   · 触发门：EVENT_HIT/EVENT_SKILL_INVALID（覆盖"使用技能"全情况——命中/miss/打盔），
//     在 ON_SKILL_HIT 桶后 drain → 早于对手 SKILL_EFFECT 桶与伤害结算。
constexpr int k2212WindowStartKey = 2212900;   // +owner：窗口起始回合
constexpr int k2212WindowRoundsKey = 2212901;  // +owner：窗口回合数
constexpr int k2212BlockSeqKey = 2212902;      // +owner：待挡的那一击（skill_use_seq）
constexpr int k2212ArmedKey = 2212903;         // +owner：常驻条目已装标记
constexpr int k2212EpochKey = 2212904;         // +owner：注册时的回合类 epoch（断回合检测）

bool mistike_2212_window_active(BattleContext* ctx, int owner) {
    auto st = ctx->plugin_storage.find(k2212WindowStartKey + owner);
    auto rd = ctx->plugin_storage.find(k2212WindowRoundsKey + owner);
    if (st == ctx->plugin_storage.end() || rd == ctx->plugin_storage.end()) {
        return false;
    }
    const int start = any_int_or(st->second);
    const int rounds = any_int_or(rd->second);
    return ctx->roundCount >= start && ctx->roundCount < start + rounds;
}

EffectResult effect_2212_guard_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || rounds <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // 窗口（不可断）+ 挡伤哨兵（可断）+ BLOCK 常驻条目，全部只在首次使用时装。
    ctx->plugin_storage[k2212WindowStartKey + owner] =
        ctx->round_effect_start_round(owner, rounds, EffectWindowKind::InRounds);
    ctx->plugin_storage[k2212WindowRoundsKey + owner] = rounds;
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, 2212, rounds, {owner},
                          [](BattleContext*, const EffectArgs&) -> EffectResult {
                              return EffectResult::kOk;   // no-op：挡伤半边的存在性凭证
                          });
    ctx->plugin_storage[k2212EpochKey + owner] = ctx->round_effect_valid_id[owner];
    if (!ctx->plugin_storage.count(k2212ArmedKey + owner)) {
        ctx->plugin_storage.emplace(k2212ArmedKey + owner, 1);
        // 触发门：对手（owner 的对手）每次使用攻击技能（威力 ≤ 阈值，面板威力）。
        for (const EventType type : {EventType::EVENT_HIT, EventType::EVENT_SKILL_INVALID}) {
            ctx->event_center_.register_watcher(
                type, owner, ctx->roundCount, /*duration_rounds=*/0, /*once=*/false,
                [owner, opp, threshold, anomaly](BattleContext* c, const BattleEvent& ev) {
                    if (!c || ev.actor != opp || !g_core || !g_core->apply_anomaly) {
                        return;
                    }
                    if (!mistike_2212_window_active(c, owner)) {
                        return;
                    }
                    // 只认**攻击技能**（视图；miss/打盔时视图已在 ON_SKILL_HIT 物化为静态类型）。
                    if (c->ws.skill_type_view[opp] == static_cast<int>(SkillType::Attribute)) {
                        return;
                    }
                    // 面板威力判据（同 854 口径：威力宝石/变威力改的是视图，条件看面板）。
                    const int idx = c->roundChoice[opp][1];
                    if (idx < 0 || idx >= static_cast<int>(c->getPet(opp).skills.size())) {
                        return;
                    }
                    if (c->getPet(opp).skills[static_cast<std::size_t>(idx)].power > threshold) {
                        return;
                    }
                    // 施加疲惫（现代通道）。成功口径（用户拍板）：落对手身上 或 弹控弹回
                    // 自己身上落了，都算成功；对手魂免/次免挡下 → 不算"未触发"（不挡伤）；
                    // 只有"最终没人被疲惫"（弹回也失败等）才算失败 → 挡伤。
                    const ApplyAnomalyResult r = g_core->apply_anomaly(
                        c, opp, anomaly, random_anomaly_duration(), owner);
                    bool need_block = false;
                    if (r == ApplyAnomalyResult::REFLECTED) {
                        // 弹回了：看疲惫有没有落到自己身上。
                        need_block = !g_core->has_active_anomaly(c, owner, anomaly);
                    } else if (r == ApplyAnomalyResult::TARGET_IMMUNE
                               || r == ApplyAnomalyResult::RESISTED_BY_RESISTANCE) {
                        need_block = false;   // 被免/抗性挡下 → 挡伤不继续生效
                    } else if (r != ApplyAnomalyResult::SUCCESS
                               && r != ApplyAnomalyResult::REPLACED_EXISTING
                               && r != ApplyAnomalyResult::DURATION_EXTENDED
                               && r != ApplyAnomalyResult::CONVERTED) {
                        need_block = false;   // 目标已死等：无从谈起
                    }
                    if (!need_block) {
                        return;
                    }
                    // 挡伤半边随哨兵走：哨兵（回合类）被断回合清掉后不再挡。
                    // （hasEffect 模板只有 0/1 显式实例化、插件用不了 → 比对注册时 epoch：
                    //   断回合 ++round_effect_valid_id，不等 = 哨兵已被作废。）
                    auto ep = c->plugin_storage.find(k2212EpochKey + owner);
                    if (ep == c->plugin_storage.end()
                        || any_int_or(ep->second) != c->round_effect_valid_id[owner]) {
                        return;
                    }
                    c->plugin_storage[k2212BlockSeqKey + owner] =
                        c->skill_use_seq[opp];   // 对准被挡的那一击
                },
                WatcherScope::TEAM);
        }
        // BLOCK 阶段常驻条目：零化"待挡那一击"的红伤（技能效果照常——效果在 SKILL_EFFECT
        // 桶已注册，BLOCK 在伤害管线内，晚于效果注册）。
        ctx->register_damage_effect(
            DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
            [owner](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != owner) {
                    return;
                }
                auto seq = c->plugin_storage.find(k2212BlockSeqKey + owner);
                if (seq == c->plugin_storage.end()) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (d.attackerId < 0
                    || any_int_or(seq->second) != c->skill_use_seq[d.attackerId]) {
                    return;   // 不是待挡那一击（含 miss 后 seq 已推进的陈旧挂起）
                }
                c->plugin_storage.erase(seq);
                d.base = 0;
                d.afterAdd = 0;
                d.afterMul = 0;
                d.final = 0;
            });
    }
    return EffectResult::kOk;
}

// ── 2214 —— 对手每有1层蚩庸之锁令对手随机1个攻击技能PP值归0，
//             对手存有PP值的攻击技能个数不足时造成的伤害提升{0}% ──
// Args=[owner, target, boost_pct]。SKILL_EFFECT 时点（她的攻击命中后才注册效果）→
// 伤害提升写 ws.damage_add_pct（ATTACK_DAMAGE 的 AMP 阶段读取）。
EffectResult effect_2214_locks_zero_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int boost_pct = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int locks = opp_lock_count(ctx, owner);
    if (locks <= 0) {
        return EffectResult::kOk;
    }
    // 有 PP 且非无限 PP 的攻击技能里随机挑（逐层不重复）。
    std::vector<int> candidates;
    ElfPet& opp_pet = ctx->getPet(opp);
    for (int i = 0; i < static_cast<int>(opp_pet.skills.size()); ++i) {
        if (opp_pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    int zeroed = 0;
    while (zeroed < locks && !candidates.empty()) {
        const int pick = ctx->rng().next() % static_cast<int>(candidates.size());
        opp_pet.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
        candidates.erase(candidates.begin() + pick);
        ++zeroed;
    }
    if (zeroed < locks && boost_pct > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(boost_pct);
    }
    return EffectResult::kOk;
}

// ── 1114 —— 消除对手回合类效果，消除成功则令对手随机{0}项技能PP值归零 ──
// Args=[owner, target, count]。随机**不重复**；只挑 pp>0 且非无限 PP 的技能。
EffectResult effect_1114_break_then_zero_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 没有可清除的回合类效果 → 后续不触发
    }
    ElfPet& opp_pet = ctx->getPet(opp);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(opp_pet.skills.size()); ++i) {
        if (opp_pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    for (int k = 0; k < count && !candidates.empty(); ++k) {
        const int pick = ctx->rng().next() % static_cast<int>(candidates.size());
        opp_pet.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
        candidates.erase(candidates.begin() + pick);
    }
    return EffectResult::kOk;
}

// ── 907 —— 反转自身能力下降状态，反转成功则使对手随机{0}个技能PP归零 ──
// Args=[owner, target, count]。只有"负等级翻正"算反转成功（无可反转的下降不触发）。
EffectResult effect_907_reverse_then_zero_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    ElfPet& opp_pet = ctx->getPet(opp);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(opp_pet.skills.size()); ++i) {
        if (opp_pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    for (int k = 0; k < count && !candidates.empty(); ++k) {
        const int pick = ctx->rng().next() % static_cast<int>(candidates.size());
        opp_pet.skills[static_cast<std::size_t>(candidates[pick])].pp = 0;
        candidates.erase(candidates.begin() + pick);
    }
    return EffectResult::kOk;
}

// ── 1827 —— 击败对手则令对手下{0}次使用的属性技能无效 ──
// Args=[owner, target, count]。监听 EVENT_DEATH（actor=击败方）匹配自己击败了对手 →
// 挂次数型封属（SEAL_ATTRIBUTE）；按条件匹配 + 自删监听器（once=true 会被无关死亡白耗）。
constexpr int k1827WatcherKey = 1827900;   // +owner：监听器已挂标记

EffectResult effect_1827_defeat_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(k1827WatcherKey + owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(k1827WatcherKey + owner, 1);
    auto watcher = std::make_shared<int>(-1);
    *watcher = ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner, count, watcher](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.target < 0 || ev.target > 1
                || ev.target == owner) {
                return;   // 只认"owner 击败了对手方"的死亡
            }
            c->event_center_.remove_watcher(*watcher);
            c->plugin_storage.erase(k1827WatcherKey + owner);
            g_core->seal_skill(c, /*source=*/owner, /*target=*/ev.target,
                               /*effect_id=*/1827, /*attribute=*/true, /*attack=*/false,
                               /*count=*/count, /*duration_rounds=*/0, /*penetrable=*/true,
                               /*source_slot=*/-1, EffectScope::ON_STAGE,
                               /*hit_invalid=*/false, /*chance_pct=*/100,
                               /*consumed_when_pierced=*/false);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ════════════════════════ 效果注册 ════════════════════════
SKILL_EFFECT(1848, effect_1848_drop_priority);

// ── 效果显示 G4（2026-11-23）：发布槽显示身份（STATE_DISPLAY_FN，规范 §4 G4）──
// 键族 = 基址+side，逐侧登记、归属编进名字（"方0/方1"）。
// ⚠️ 必须用插件侧解码器（STATE_DISPLAY_FN）而不是 kind 通用解码：
//    std::any 的 type_info/manager 是各 TU 独立实例，内核 generic 解码 any_cast<int>
//    对插件写入的值必抛 bad_any_cast（2026-11-23 实证值恒 "?"）。同 dylib 读写才安全。
namespace {
bool mistike_decode_int(const std::any& stored, DecodedValue& out) {
    try {
        std::snprintf(out.text, sizeof(out.text), "%d", std::any_cast<int>(stored));
        return true;
    } catch (...) {
        return false;
    }
}
}  // namespace
STATE_DISPLAY_FN(4676000, "蚩庸之锁层数(方0)", mistike_decode_int);
STATE_DISPLAY_FN(4676001, "蚩庸之锁层数(方1)", mistike_decode_int);
STATE_DISPLAY_FN(4676500, "蚩庸之锁槽位(方0)", mistike_decode_int);
STATE_DISPLAY_FN(4676501, "蚩庸之锁槽位(方1)", mistike_decode_int);          // 心朽魂凋：对手有下降→先制+2（选择期）
SKILL_EFFECT(449, effect_449_conditional_anomaly);      // 心朽魂凋：对手有下降→100%疲惫
SKILL_EFFECT(1684, effect_1684_lock_drops);             // 心朽魂凋：对手下降不可解除/反转
SKILL_EFFECT(1428, effect_1428_all_down_then_roundly);  // 心朽魂凋：全属性-1+每回合三项-1
SKILL_EFFECT(1338, effect_1338_all_up_maybe_double);    // 魂劫骇咒：全属性+1（对手有下降翻倍）
SKILL_EFFECT(2076, effect_2076_drain_window);           // 魂劫骇咒：4回合每技吸取1/3
SKILL_EFFECT(1471, effect_1471_clear_then_fatigue);     // 溃梦灵言：消强→疲惫/未触发回1/3
SKILL_EFFECT(2212, effect_2212_guard_window);           // 溃梦灵言：受击疲惫控+挡伤
SKILL_EFFECT(2148, effect_2148_side_anomaly_immune);    // 堕之盛响：己方免疫下1次异常（传承）
SKILL_EFFECT(2211, effect_2211_add_locks);              // 蚩庸之锁：附加/超上限先制-1
SKILL_EFFECT(2213, effect_2213_self_destruct);          // 堕之盛响：自爆传承盾罩
SKILL_EFFECT(2214, effect_2214_locks_zero_pp);          // 王技：每层锁→随机攻击技PP归零
SKILL_EFFECT(1114, effect_1114_break_then_zero_pp);     // 王技：消回合成功→随机2项PP归零
SKILL_EFFECT(907, effect_907_reverse_then_zero_pp);     // 王技：反转下降→随机2项PP归零
SKILL_EFFECT(1827, effect_1827_defeat_seal);            // 王技：击败→对手下1次属性技能无效
