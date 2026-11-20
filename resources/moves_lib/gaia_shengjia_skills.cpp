// 圣甲·盖亚 3056 技能组 —— 缺口效果族（DB 模板已核对 effect_info，2026-09-20）
//
// 套件 20 技中 13 个效果号引擎已有实现（4/5/31/33/43/48/58/60/191/408/478/481/521/585/776），
// 本文件补 8 个缺口；第 9 个新原语 cure_anomalies（544 需要的选择性解异常）在 core。
//   402  后出手时，额外附加{0}点固定伤害           —— 圣战之拳 [100]
//   456  若对手体力不足{0}则直接秒杀               —— 天威凛怒拳 [300]
//   459  附加对手防御值{0}%的百分比伤害             —— 碎星机动拳 [40]
//   544  解除自身的烧伤、冻伤、中毒状态             —— 逆转机甲（其余段 3/43/58/481 已有）
//   806  {0}回合内若对手使用攻击技能则使用后令自身全属性+{1} —— 霸威冠宇 [3,1]
//   826  当回合击败对手则下回合开始后随机附加{0}种异常状态   —— 天威凛怒拳 [1]
//   859  吸收对手能力提升，吸收成功减少对手1/{0}最大体力      —— 狂绝冲撞 [3]
//   894  吸取对手{0}点体力，体力低于对手时吸取效果翻倍        —— 圣甲气魄 [200]
//
// 口径备注：
//   ★ 459 是"**百分比伤害**"档（吃百分比抗性/护罩）——数值 = 对手防御值×{0}%，
//     走 PERCENT_VALUE（值已算好、档位仍属百分比）。防御值读本体 DEF（等级变动场景待实测）。
//   ★ 806 的"对手使用攻击技能"按**对手行动结束**结算：窗口节点镜像在双方 AFTER_ACTION，
//     节点内用 currentState × is_second_mover 判定当前出手方是不是对手（出手位施放时未知）。
//   ★ 826 的"当回合击败"用 MOVER_DEATH 桶节点探测（对手 0 血 + last_damage_actor=自己），
//     命中则立旗；"下回合开始后"由 NextRounds 的 ROUND_START 节点消费旗并随机附加。
//   ★ 859 的"吸收"= 把对手的**正等级**转移给自己（stat_change 双向写，本体/视图同步）；
//     吸收总量 > 0 才算成功。894 的吸取回血按**结果口径**（实际落到体力的量，同 1892 修正）。
#include "common.h"

namespace {

// 826 的击败旗（plugin_storage int：值=施放回合数；ROUND_START 消费后清除）。
// ⚠️ 键段 3056300 = **826 独占**（2026-10-22 修跨 dylib 撞键）。
//    原用 3056200 = gaia_runtime.h 的 kGaiaResolvingKey（魂印侧"死亡判定求解中守卫"，
//    用法 `kGaiaResolvingKey + side` → 占 3056200/3056201）。两边都是 int 但**同一个
//    map 的同一个键** → 魂印侧写守卫会把这边的旗值盖成 1、再 erase 掉 → 826 的
//    "击败旗"凭空消失（技能不触发）；反过来魂印侧读
//    `count(kGaiaResolvingKey + gaia_side)` 会把这边的旗当成"还在求解中" → 误判反同归。
//    这处是 tools/check_storage_keys.py 抓到的真实跨 dylib 撞键。
constexpr int kGaia826ArmKey = 3056300;

// ── 402 —— 后出手时，额外附加{0}点固定伤害 ──
EffectResult effect_402_second_mover_extra(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 先出手：不附加
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 456 —— 若对手体力不足{0}则直接秒杀（归零原语：护盾/护罩/减伤不参与）──
EffectResult effect_456_low_hp_execute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->force_hp_to_zero) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || threshold <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp >= threshold) {
        return EffectResult::kOk;   // 体力充足：不触发
    }
    g_core->force_hp_to_zero(ctx, target, owner);
    return EffectResult::kOk;
}

// ── 459 —— 附加对手防御值{0}%的百分比伤害（PERCENT_VALUE：值已算好、档位属百分比）──
EffectResult effect_459_def_scaling_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int def =
        ctx->getPet(target).numericalProperties[NumericalPropertyIndex::DEFENSE];
    const int value = def * pct / 100;
    if (value <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, value, DamageKind::PERCENT_VALUE, owner);
    return EffectResult::kOk;
}

// ── 544 —— 解除自身的烧伤、冻伤、中毒状态（名单外如麻痹/睡眠必须留下）──
EffectResult effect_544_cure_triple(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core || !g_core->cure_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int triple[] = {static_cast<int>(AbnormalStatusId::Burn),
                          static_cast<int>(AbnormalStatusId::Frostbite),
                          static_cast<int>(AbnormalStatusId::Poison)};
    g_core->cure_anomalies(ctx, owner, triple, 3);
    return EffectResult::kOk;
}

// ── 806 的窗口节点：本时点是**对手**的行动结束、且对手这回合用了攻击技能 → 自身全属性+{1} ──
EffectResult effect_806_watch(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int boost = args.int_args[1];
    if (owner < 0 || owner > 1 || boost == 0) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    // 当前 AFTER_ACTION 时点属于谁：FIRST ↔ 先手方。is_second_mover 给出 0 是否后手。
    const bool first_flow = ctx->currentState == State::BATTLE_FIRST_AFTER_ACTION;
    const int acting = (first_flow != ctx->is_second_mover(0)) ? 0 : 1;
    if (acting != opp) {
        return EffectResult::kOk;   // 自己的行动结束：不触发
    }
    if (!executed_skill_this_round(ctx, opp) || !executed_skill_is_attack(ctx, opp)) {
        return EffectResult::kOk;   // 对手没用攻击技能（属性技/嗑药/被控不算）
    }
    all_stats_up(ctx, owner, boost);
    return EffectResult::kOk;
}

// ── 806 —— {0}回合内若对手使用攻击技能则使用后令自身全属性+{1} ──
EffectResult effect_806_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int boost = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // ⚠️ AFTER_ACTION 时点桶按**行动方 robotId** 执行（execute_registered_actions(side, state)）
    // → "对手行动后"的节点必须挂**对手名下**；出手位施放时未知 → 两个时点都挂对手名下，
    //   只有对手真正行动的那个时点会执行（tick 内再按 acting==opp 把一道关）。
    const std::vector<int> tick_args{owner, boost};
    register_round_effect(ctx, State::BATTLE_FIRST_AFTER_ACTION, 1 - owner, 806, rounds,
                          tick_args, &effect_806_watch);
    register_round_effect(ctx, State::BATTLE_SECOND_AFTER_ACTION, 1 - owner, 806, rounds,
                          tick_args, &effect_806_watch);
    return EffectResult::kOk;
}

// ── 826 的 ROUND_START 消费节点：旗在 → 给对手随机附加{count}种异常 ──
EffectResult effect_826_apply(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->attach_random_anomalies) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[1];
    auto it = ctx->plugin_storage.find(kGaia826ArmKey);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) == 0) {
        return EffectResult::kOk;   // 上回合没击败对手：旗不在
    }
    ctx->plugin_storage.erase(kGaia826ArmKey);
    g_core->attach_random_anomalies(ctx, 1 - owner, anomaly_pool_all(), count, 100, owner,
                                    ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 826 的 MOVER_DEATH 探测节点：对手 0 血且被自己击败 → 立旗 ──
EffectResult effect_826_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = 1 - owner;
    const int slot = ctx->on_stage[opp];
    if (slot < 0 || slot >= 6 || ctx->seerRobot[opp].elfPets[slot].hp > 0) {
        return EffectResult::kOk;
    }
    if (ctx->last_damage_actor[opp][slot] != owner) {
        return EffectResult::kOk;   // 不是自己击败的
    }
    ctx->plugin_storage[kGaia826ArmKey] = 1;
    return EffectResult::kOk;
}

// ── 826 —— 当回合击败对手则下回合开始后随机附加{0}种异常状态 ──
EffectResult effect_826_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    // 本回合的死亡探测（两个 MOVER_DEATH 时点都挂——击败发生在哪个出手位都算）
    const std::vector<int> arm_args{owner};
    register_round_effect(ctx, State::BATTLE_FIRST_MOVER_DEATH, owner, 826, 1,
                          arm_args, &effect_826_arm);
    register_round_effect(ctx, State::BATTLE_SECOND_MOVER_DEATH, owner, 826, 1,
                          arm_args, &effect_826_arm);
    // 下回合开始后消费（NextRounds：本回合不算）
    const std::vector<int> apply_args{owner, count};
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 826, 1,
                                apply_args, &effect_826_apply);
    return EffectResult::kOk;
}

// ── 859 —— 吸收对手能力提升，吸收成功减少对手1/{0}最大体力 ──
EffectResult effect_859_absorb_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_change
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    int absorbed = 0;
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        const int lvl = ctx->ability_levels[target][slot];
        if (lvl <= 0) {
            continue;   // 只吸正等级（弱化不吸）
        }
        g_core->stat_change(ctx, target, slot, -lvl);
        g_core->stat_change(ctx, owner, slot, +lvl);
        absorbed += lvl;
    }
    if (absorbed <= 0) {
        return EffectResult::kOk;   // 吸收失败：不扣血
    }
    // "减少对手1/{denom}最大体力"：走百分比档（amount = 100/denom，PERCENT 档按
    // max_hp×amount/100 换算，1/3 → 33% 向下取整）。
    g_core->deal_pink_damage(ctx, target, 100 / denom, DamageKind::PERCENT, owner);
    return EffectResult::kOk;
}

// ── 894 —— 吸取对手{0}点体力，体力低于对手时吸取效果翻倍 ──
EffectResult effect_894_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    int drain = amount;
    if (ctx->getPet(owner).hp < ctx->getPet(target).hp) {
        drain *= 2;   // 体力低于对手 → 翻倍
    }
    const int hp_before = ctx->getPet(target).hp;
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::FIXED, owner);
    const int received = hp_before - ctx->getPet(target).hp;
    if (received > 0) {
        g_core->heal_amount(ctx, owner, received);   // 按实际落到体力的量回（结果口径）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(402, effect_402_second_mover_extra);
SKILL_EFFECT(456, effect_456_low_hp_execute);
SKILL_EFFECT(459, effect_459_def_scaling_pink);
SKILL_EFFECT(544, effect_544_cure_triple);
SKILL_EFFECT(806, effect_806_window);
SKILL_EFFECT(826, effect_826_window);
SKILL_EFFECT(859, effect_859_absorb_boosts);
SKILL_EFFECT(894, effect_894_drain);
