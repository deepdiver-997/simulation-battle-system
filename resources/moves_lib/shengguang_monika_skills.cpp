#include <utils/sbs_rand.h>
// 圣光莫妮卡（神谕 3711）技能组（精灵王线 2026-09-24；技能组补全 2026-09-25）
//   2425 王·圣语空心：攻击伤害额外提升{0}%，自身与对手每有{1}个技能PP值不高于{2}再+{3}%
//   2426 王·圣语空心：击败对手时消耗自身一半律，每剩余1道→对手1回合无法造成伤害+附加异常
//   2427 王·鸾歌余音：消耗自身一半律，每剩余1道→对手1回合属性技能命中效果失效
//   2428 繁嚣此寂：律 < 己方琼华之庇数 → 补齐到庇数
//   2429 万籁悦性：对手处于回合类效果时令对手{0}项技能PP归0
//   2430 王·圣语空心：未造成攻击伤害时消除对手回合类效果并令对手{0}
//   2432 王·圣语空心：每造成{0}次攻击伤害，己方下只精灵出战时随机恢复{1}点PP
// ── 技能组补全（2026-09-25，旧技 + 神谕新技共享效果；文本以 effect_info 为准）──
//   710  王·落叶知秋：解除自身能力下降，成功则{0}回合免疫能力下降
//   720  碧影清风：若先出手则当回合闪避对手的攻击技能
//   736  万籁(俱寂/悦性)：消除对手能力提升，成功则{0}回合内对手使用的属性技能无效
//   738  神灵涤荡：{0}回合内对手使用属性技能→其下{1}回合攻击技能无法造成伤害且命中效果失效
//   879  万籁悦性：消除对手回合类效果，成功则恢复自身全部体力
//   939  碧影清风：若后出手则附加自身最大体力1/{0}的百分比伤害并恢复等量体力
//   1054 王·鸾歌余音：反转自身能力下降，反转成功则对手{0=异常号}
//   1382 万籁俱寂：命中后获得{0}层自然祝福，伤害高于{1}则额外+{2}层（**自然祝福**，非律）
//   1383 清溦芳华：全属性+{0}，处于自然祝福状态时强化翻倍（判**自然祝福**）
//   1384 清溦芳华：获得{0}层自然祝福（**自然祝福**，非律）
//   1385 王·落叶知秋：消耗自身所有自然祝福，每消耗1层随机令对手1个技能PP归零（**自然祝福**）
//   1485 万籁悦性：对手处于提升状态则命中后{0}%令对手{1=异常号}，未触发则{2}%令对手{3=异常号}
//   1558 繁嚣此寂：下{0}回合若打出致命一击则造成的攻击伤害提升{1}%
//   1909 王·鸾歌余音：令自身体力等于最大体力的{0}%，若高于最大体力的{1}%则回合结束后
//        额外触发一次该效果（体力重置≠恢复：走 reset_hp 原语，无视任何减疗/封回血，
//        不发回血事件——用户 2026-09-25 口径）
//   2380 繁嚣此寂：自身下{0}次致命一击视为命中并强制执行命中效果
//        （消费点在内核 query_usage 暴击判定处 990238000+owner 计数）
#include "common.h"
#include "../monika_runtime.h"

namespace {

constexpr int kMark = 2492;

// ── 2425 —— 伤害提升 + 双方低 PP 槽计数加成（AMP_EXTRA 加在攻击前）──
EffectResult effect_2425_amp_by_pp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int base = args.int_args[2];
    const int per = args.int_args[3];
    const int threshold = args.int_args[4];
    const int bonus = args.int_args[5];
    auto low = [&](int side) {
        int n = 0;
        for (const Skills& s : ctx->getPet(side).skills) {
            if (s.pp != -1 && s.pp <= threshold) {
                ++n;
            }
        }
        return n;
    };
    const int stacks = low(owner) + low(1 - owner);
    ctx->ws.damage_add_extra_mul[owner][0] += base + per * stacks;
    return EffectResult::kOk;
}

// ── 2426 —— 击败：耗一半律，每剩余 1 道 → 对手 1 回合无法造成伤害+附加异常 ──
EffectResult effect_2426_kill_lv(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (ctx->getPet(target).hp > 0) {
        return EffectResult::kOk;
    }
    // 消耗口径（用户 2026-09-25）：消耗 = (int)(lv/2)，剩余 = lv - (int)(lv/2)——
    // 奇数道不丢失：1 道时消耗 0 但仍有 1 回合效果。
    const int lv = moni_pub_get(ctx->plugin_storage, kMoniPubLv + owner);
    const int remaining = lv - lv / 2;
    moni_pub_set(ctx->plugin_storage, kMoniPubLv + owner, remaining);
    if (remaining > 0) {
        // "每剩余1道→对手1回合无法造成伤害"：属/物攻各挂一个 remaining 回合窗口
        if (g_core->seal_skill) {
            for (const bool attr : {true, false}) {
                g_core->seal_skill(ctx, owner, target, 2426, attr, !attr, 0,
                                   remaining, false, ctx->on_stage[owner], EffectScope::ON_STAGE,
                                   true, 100, false);
            }
        }
        if (g_core->apply_anomaly) {
            g_core->apply_anomaly(ctx, target, anomaly, 1, owner);
        }
    }
    return EffectResult::kOk;
}

// ── 2427 —— 耗一半律，每剩余 1 道 → 对手 1 回合属性技命中失效 ──
EffectResult effect_2427_lv_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || !g_core
        || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // 消耗口径（用户 2026-09-25）：消耗 = (int)(lv/2)，剩余 = lv - (int)(lv/2)——
    // 奇数道不丢失：1 道时消耗 0 但仍有 1 回合属性技命中失效。
    const int lv = moni_pub_get(ctx->plugin_storage, kMoniPubLv + owner);
    const int remaining = lv - lv / 2;
    moni_pub_set(ctx->plugin_storage, kMoniPubLv + owner, remaining);
    for (int i = 0; i < remaining; ++i) {
        g_core->seal_skill(ctx, owner, target, 2427, /*attribute=*/true, /*attack=*/false,
                           /*count=*/1, 0, false, ctx->on_stage[owner], EffectScope::ON_STAGE,
                           /*hit_invalid=*/true, 100, false);
    }
    return EffectResult::kOk;
}

// ── 2428 —— 律 < 己方庇数 → 补齐 ──
EffectResult effect_2428_sync_lv(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int lv = moni_pub_get(ctx->plugin_storage, kMoniPubLv + owner);
    const int bi = moni_pub_get(ctx->plugin_storage, kMoniPubBi + owner);
    if (lv < bi) {
        moni_pub_set(ctx->plugin_storage, kMoniPubLv + owner, bi);
    }
    return EffectResult::kOk;
}

// ── 2429 —— 对手处于回合类效果 → {0}项 PP 归 0 ──
EffectResult effect_2429_pp_zero_cond(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int slots_n = args.int_args[2];
    if (!ctx->has_round_effects(target)) {
        return EffectResult::kOk;
    }
    int idx[5] = {0, 1, 2, 3, 4};
    for (int i = 4; i > 0; --i) {
        const int j = ctx->rng().next() % (i + 1);
        std::swap(idx[i], idx[j]);
    }
    for (int i = 0; i < slots_n && i < 5; ++i) {
        g_core->pp_zero_slot(ctx, target, idx[i], /*actor=*/owner);
    }
    return EffectResult::kOk;
}

// ── 2430 —— 未造成攻击伤害 → 消除对手回合类效果并令对手{0} ──
EffectResult effect_2430_no_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (ctx->ws.raw_attack_damage[owner] > 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    if (anomaly > 0 && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, target, anomaly, 1, owner);
    }
    return EffectResult::kOk;
}

// ── 2432 —— 每{0}次攻击伤害 → 己方下只出战随机恢复{1}点 PP（记账在魂印 storage）──
EffectResult effect_2432_pp_inherit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int per = args.int_args[2];
    const int points = args.int_args[3];
    auto& store = ctx->getPet(owner).soulmark_storage;
    const int key = 2432000 + owner;
    int hits = store.count(key) ? any_int_or(store[key]) : 0;
    ++hits;
    store[key] = hits;
    if (hits % per == 0 && g_core && g_core->pp_restore_points) {
        // 记到"下只出战精灵"账上：side 级待领槽（下只出战时由其魂印领取——
        // 当前无领取方，先落 side 槽，圣莫在场时由本魂印 ROUND_START 代领给自身随机槽）
        const int key2 = 2432100 + owner;
        int banked = store.count(key2) ? any_int_or(store[key2]) : 0;
        store[key2] = banked + points;
    }
    (void)points;
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 技能组补全（2026-09-25）。⚠️ 旧技能族 1382/1383/1384/1385 字面操作**自然祝福**
// （未神谕旧资源，kMoniPubNz，上限 4）——与宁界之律（kMoniPubLv，上限 5）**互不相通**
// （用户 2026-09-25 口径：新旧资源不混用）；律的消耗/补齐只在新技效果 2426/2427/2428。
// ════════════════════════════════════════════════════════════════════
constexpr int kNzCap = 4;   // 自然祝福上限（旧文本"最多可获得4层"）

inline void monika_gain_nz(BattleContext* ctx, int owner, int n) {
    const int nz = moni_pub_get(ctx->plugin_storage, kMoniPubNz + owner);
    moni_pub_set(ctx->plugin_storage, kMoniPubNz + owner,
                 nz + n > kNzCap ? kNzCap : nz + n);
}

// ── 710 王·落叶知秋 —— 解除自身能力下降，成功则{0}回合免疫能力下降 ──
// "解除"= clear_stat_drops（消负等级，非反转；不查免疫是原语既有口径）。
EffectResult effect_710_clear_drops_then_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, owner) <= 0) {
        return EffectResult::kOk;   // 无下降可解 → 解除未成功
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                        /*anomaly_mask=*/0, rounds, /*source_id=*/0, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/710);
    return EffectResult::kOk;
}

// ── 720 碧影清风 —— 若先出手则当回合闪避对手的攻击技能 ──
// ws.dodge_rate 每回合 reset → 只写当回合，天然"当回合"作用域（426 同槽）。
EffectResult effect_720_dodge_if_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1 || ctx->is_second_mover(owner)) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[owner] = 1.0f;
    return EffectResult::kOk;
}

// ── 736 万籁俱寂/万籁悦性 —— 消除对手能力提升，成功则{0}回合内对手属性技能无效 ──
// 478（"{0}回合内令对手使用的属性技能无效"家族）同款 seal_skill 回合型。
EffectResult effect_736_clear_boost_then_seal_attr(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts
        || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, target) <= 0) {
        return EffectResult::kOk;   // 消强未成功（无提升/免消除）→ 不封属
    }
    g_core->seal_skill(ctx, /*source=*/owner, /*target=*/target, /*effect_id=*/736,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 738 神灵涤荡 —— {0}回合内对手使用属性技能 → 其下{1}回合攻击白板（无法造成伤害
//    且命中效果失效）。监视 = 对手 AFTER_ACTION 桶（2099 敕界归元同款）；
//    触发一次即收窗。"下{1}回合"以命中失效 {1} 次近似（≈每回合 1 次攻击；
//    与 2427 "每剩余1道→1回合"同款口径，多段攻击会多耗——偏差记交接文档待实测）。
EffectResult effect_738_watch_tick(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_738_watch_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->hit_effect_invalid) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = 1 - owner;
    // tick 挂在对手桶里跑 → 判"对手本回合是不是用了属性技能"
    if (!executed_skill_this_round(ctx, opp) || executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[990738000 + owner] = 1;   // 触发一次即收窗（窗口到期自清）
    g_core->hit_effect_invalid(ctx, opp, HitInvalidMode::kFullNull,
                               /*count=*/args.int_args[1], /*is_attribute_skill=*/false, 738);
    return EffectResult::kOk;
}

EffectResult effect_738_attr_watch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int watch_rounds = args.int_args[2];
    const int null_rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || watch_rounds <= 0 || null_rounds <= 0) {
        return EffectResult::kOk;
    }
    const std::vector<int> tick{owner, null_rounds};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, /*owner=*/1 - owner, 738,
                          watch_rounds, tick, &effect_738_watch_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, /*owner=*/1 - owner, 738,
                          watch_rounds, tick, &effect_738_watch_tick);
    return EffectResult::kOk;
}

// ── 879 万籁悦性 —— 消除对手回合类效果，成功则恢复自身全部体力（945 同文异号）──
EffectResult effect_879_break_then_full_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->break_round_effects || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    g_core->heal_amount(ctx, self, max_hp_of(ctx, self));
    return EffectResult::kOk;
}

// ── 939 碧影清风 —— 若后出手则附加自身最大体力1/{0}的百分比伤害并恢复等量体力 ──
EffectResult effect_939_second_mover_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0
        || !ctx->is_second_mover(owner)) {
        return EffectResult::kOk;
    }
    const int value = max_hp_of(ctx, owner) / denom;
    if (value <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, value, DamageKind::PERCENT_VALUE, owner);
    g_core->heal_amount(ctx, owner, value);
    return EffectResult::kOk;
}

// ── 1054 王·鸾歌余音 —— 反转自身能力下降，反转成功则对手{0}（异常号 40=空定）──
// ⚠️ {0} 按"令对手{异常号}"族解读（1047/1485 同文式）；40=VoidFixation 空定，待实测。
EffectResult effect_1054_reverse_drops_then_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_reversal
        || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 反转失败（无下降/被锁）→ 不附加
    }
    g_core->apply_anomaly(ctx, target, anomaly, 1, owner);
    return EffectResult::kOk;
}

// ── 1382 万籁俱寂 —— 命中后获得{0}层自然祝福，若造成的伤害高于{1}则额外+{2}层 ──
// AFTER_ACTION 时点（内核 effect_register_state case 1382）→ resolvedDamage.final 可读。
EffectResult effect_1382_lv_on_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int gain = args.int_args[2];
    const int threshold = args.int_args[3];
    const int extra = args.int_args[4];
    if (owner < 0 || owner > 1 || gain <= 0) {
        return EffectResult::kOk;
    }
    int n = gain;
    if (extra > 0 && ctx->resolvedDamage.final > threshold) {
        n += extra;
    }
    monika_gain_nz(ctx, owner, n);
    return EffectResult::kOk;
}

// ── 1383 清溦芳华 —— 全属性+{0}，自身处于自然祝福状态（自然祝福 > 0）时强化翻倍 ──
EffectResult effect_1383_all_up_maybe_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int delta = args.int_args[2];
    if (owner < 0 || owner > 1 || delta <= 0) {
        return EffectResult::kOk;
    }
    const int mult =
        moni_pub_get(ctx->plugin_storage, kMoniPubNz + owner) > 0 ? 2 : 1;
    all_stats_up(ctx, owner, delta * mult);
    return EffectResult::kOk;
}

// ── 1384 清溦芳华 —— 获得{0}层自然祝福 ──
EffectResult effect_1384_gain_lv(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int gain = args.int_args[2];
    if (owner < 0 || owner > 1 || gain <= 0) {
        return EffectResult::kOk;
    }
    monika_gain_nz(ctx, owner, gain);
    return EffectResult::kOk;
}

// ── 1385 王·落叶知秋 —— 消耗自身所有自然祝福，每消耗1层随机令对手1个技能PP值归零 ──
EffectResult effect_1385_consume_all_lv_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int nz = moni_pub_get(ctx->plugin_storage, kMoniPubNz + owner);
    moni_pub_set(ctx->plugin_storage, kMoniPubNz + owner, 0);
    int idx[5] = {0, 1, 2, 3, 4};
    for (int i = 4; i > 0; --i) {
        const int j = ctx->rng().next() % (i + 1);
        std::swap(idx[i], idx[j]);
    }
    for (int i = 0; i < nz && i < 5; ++i) {
        g_core->pp_zero_slot(ctx, target, idx[i], /*actor=*/owner);
    }
    return EffectResult::kOk;
}

// ── 1485 万籁悦性 —— 对手处于提升状态则命中后{0}%令对手{1}，未触发则{2}%令对手{3} ──
// {1}/{3} = 异常号（8=睡眠、40=空定；1047 同文式口径）。
EffectResult effect_1485_conditional_anomaly_pair(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct_a = args.int_args[2];
    const int anomaly_a = args.int_args[3];
    const int pct_b = args.int_args[4];
    const int anomaly_b = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    bool boosted = false;
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(target, stat) > 0) {
            boosted = true;
            break;
        }
    }
    if (boosted) {
        if (pct_a > 0 && (ctx->rng().next() % 100) < pct_a) {
            g_core->apply_anomaly(ctx, target, anomaly_a, 1, owner);
        }
    } else {
        if (pct_b > 0 && (ctx->rng().next() % 100) < pct_b) {
            g_core->apply_anomaly(ctx, target, anomaly_b, 1, owner);
        }
    }
    return EffectResult::kOk;
}

// ── 1558 繁嚣此寂 —— 下{0}回合若打出致命一击则造成的攻击伤害提升{1}% ──
// 暴击倍率槽 ws.cached_crit_damage（默认 200）在窗口内每回合 ROUND_START 乘算放大；
// 只影响暴击落地的那几刀（非暴击不吃），与"若暴击则提升"字面一致。窗口用 NextRounds。
EffectResult effect_1558_crit_amp_tick(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_1558_crit_amp_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.cached_crit_damage[owner] =
        ctx->ws.cached_crit_damage[owner] * (100 + bonus) / 100;
    return EffectResult::kOk;
}

EffectResult effect_1558_crit_amp_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || bonus <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1558, rounds,
                                {owner, bonus}, &effect_1558_crit_amp_tick);
    return EffectResult::kOk;
}

// ── 1909 王·鸾歌余音 —— 体力重置：体力等于最大体力的{0}%；当前高于最大体力的{1}%
//    则回合结束后额外触发一次（=本回合再被打瘦身后回合末补满）。走 reset_hp 原语
//    （重置≠恢复：无视减疗/封回血、不发回血事件，见 battle_primitives.h 口径）。
EffectResult effect_1909_retrigger_tick(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_1909_retrigger_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->reset_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[1];
    const int threshold_pct = args.int_args[2];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, owner);
    if (ctx->getPet(owner).hp > max_hp * threshold_pct / 100) {
        g_core->reset_hp(ctx, owner, pct);
    }
    return EffectResult::kOk;
}

EffectResult effect_1909_reset_hp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->reset_hp) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    const int threshold_pct = args.int_args[3];
    if (owner < 0 || owner > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    g_core->reset_hp(ctx, owner, pct);
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1909, /*rounds=*/1,
                          {owner, pct, threshold_pct}, &effect_1909_retrigger_tick);
    return EffectResult::kOk;
}

// ── 2380 繁嚣此寂 —— 自身下{0}次致命一击视为命中并强制执行命中效果 ──
// 武装计数（990238000+owner）；消费点 = 内核 query_usage 暴击判定（skills.cpp，
// 暴击落地 → cred.force_execute：隐含必中 + ③层/盔威绕过）。重复武装按官方
// "下{0}次"字面刷新计数（待实测：是否叠加）。
EffectResult effect_2380_arm_crit_execute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int charges = args.int_args[2];
    if (owner < 0 || owner > 1 || charges <= 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[990238000 + owner] = charges;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2425, effect_2425_amp_by_pp);
SKILL_EFFECT(2426, effect_2426_kill_lv);
SKILL_EFFECT(2427, effect_2427_lv_seal);
SKILL_EFFECT(2428, effect_2428_sync_lv);
SKILL_EFFECT(2429, effect_2429_pp_zero_cond);
SKILL_EFFECT(2430, effect_2430_no_damage);
SKILL_EFFECT(2432, effect_2432_pp_inherit);
// ── 技能组补全（2026-09-25）──
SKILL_EFFECT(710, effect_710_clear_drops_then_immune);
SKILL_EFFECT(720, effect_720_dodge_if_first);
SKILL_EFFECT(736, effect_736_clear_boost_then_seal_attr);
SKILL_EFFECT(738, effect_738_attr_watch);
SKILL_EFFECT(879, effect_879_break_then_full_heal);
SKILL_EFFECT(939, effect_939_second_mover_drain);
SKILL_EFFECT(1054, effect_1054_reverse_drops_then_anomaly);
SKILL_EFFECT(1382, effect_1382_lv_on_damage);
SKILL_EFFECT(1383, effect_1383_all_up_maybe_double);
SKILL_EFFECT(1384, effect_1384_gain_lv);
SKILL_EFFECT(1385, effect_1385_consume_all_lv_pp_zero);
SKILL_EFFECT(1485, effect_1485_conditional_anomaly_pair);
SKILL_EFFECT(1558, effect_1558_crit_amp_window);
SKILL_EFFECT(1909, effect_1909_reset_hp);
SKILL_EFFECT(2380, effect_2380_arm_crit_execute);
