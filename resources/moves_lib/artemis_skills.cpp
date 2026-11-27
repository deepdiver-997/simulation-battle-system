// 阿尔忒弥斯 4745 技能组（2026-09-20）
//
// 官方技能全文 = reference/scrape_4399/out/pets_full.jsonl（阿尔忒弥斯节）；模板 = DB effect_info。
// **狩杀之的/狩杀之魂** 状态由魂印侧（soul_lib/artemis_4745.cpp）维护，本文件按同一套键读写：
//   狩杀之的标记 side/slot = 990474511/990474512（挂在**她**的 storage）；
//   狩杀之魂层数 = 990474501。
//
// 已实现：
//   37860 穿林响（先三 85）：1445 吸取对手提升→成功则2回合封对手属性技；对手无提升则自身
//        下N回合先制+2 / 2273 对手选择攻击技能时此技能先制+3且{0}%令对手{1}
//   37861 抿叶扬音：1545 消除对手护盾（成功则为自身附加等量护盾）/ 2198 双方任一方有护盾
//        时{0}%令对手{1}
//   37862 信步嶙峋（140）：405 先出手额外附加{0}点固定伤害 / 721 后出手使对手下{0}回合
//        攻击技能MISS
//   37863 森语百兽谣（150）：881 自身处于能力下降则先制+3 / 1279 反转自身下降成功则使对手
//        下{0}次技能无效 / 2278 狩杀之的增伤联动（含狩杀之魂提升）
//   29004 神驰天苍 / 29006 翎落地茫：2276/2277 **标记狩杀之的**（对方场下克制系数最高/最低
//        的非狩杀之的精灵）/ 433(全属性+1) / 2274 TODO(回满PP+吸体力) / 2009 异常→封属性 /
//        481(已有·先制+2)
//
// TODO（需新引擎能力，逐条留痕）：
//   2274 神驰天苍/翎落地茫「恢复自身全部PP值，每恢复1点吸取对手30点体力」——需"实际恢复的
//        PP 点数"台账（PP 恢复量在引擎里无统一记账点）；
//   2275/2279 挽风游天歌「流血/未触发必暴/击败狩杀之的时下只被标记」——流血状态族与
//        "击败后标记下只"需 EVENT_DEATH 归因接线（击杀线已有 find_defeated_foe_slot 可复用）；
//   1600(已有·流血) / 1747(已有·消回合)。
#include "common.h"

namespace {

// ⚠️ 与 soul_lib/artemis_4745.cpp 同一注册键（effect_icon.effect_id = 2354）。
constexpr int kArtemisMarkId = 2354;
// 狩杀之的标记键（与 soul_lib/artemis_4745.cpp 一致）。
constexpr int kMarkSideKey = 990474511;
constexpr int kMarkSlotKey = 990474512;
constexpr int kHuntSoulKey = 990474501;

// 狩杀之的槽（-1 = 无/已失效）——与她魂印侧同判据（存活且未消逝）。
int marked_slot_of(BattleContext* ctx, int owner) {
    const int slot = ctx->on_stage[owner];
    auto& storage = ctx->seerRobot[owner].elfPets[slot].soulmark_storage;
    auto sit = storage.find(kMarkSideKey);
    auto lit = storage.find(kMarkSlotKey);
    if (sit == storage.end() || lit == storage.end()) {
        return -1;
    }
    const int side = any_int_or(sit->second);
    const int mslot = any_int_or(lit->second);
    if (side < 0 || side > 1 || mslot < 0 || mslot > 5) {
        return -1;
    }
    if (ctx->seerRobot[side].elfPets[mslot].hp <= 0 || ctx->is_vanished(side, mslot)) {
        return -1;
    }
    return mslot;
}

int hunt_soul_of(BattleContext* ctx, int owner) {
    auto& storage = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].soulmark_storage;
    auto it = storage.find(kHuntSoulKey);
    return it == storage.end() ? 0 : any_int_or(it->second);
}

// ── 1279 反转自身能力下降状态，反转成功则使对手下{0}次技能无效 ──
// Args=[owner,target,counts]。反转走真原语（stat_reversal 对自身下降）；成功 → 对手侧挂
// 技能拦截（攻击+属性都封，次数型 counts 由引擎消费）。
EffectResult effect_1279_reverse_then_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->stat_reversal || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, owner) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 无下降可反转 → 不封
    }
    // "使对手下{0}次技能无效"：攻击+属性都封（两次挂载，各按类型响应）。
    g_core->seal_skill(ctx, owner, target, /*effect_id=*/1279, /*attribute=*/true,
                       /*attack=*/false, counts, /*duration_rounds=*/0,
                       /*penetrable=*/false, /*source_slot=*/-1, EffectScope::ON_STAGE,
                       /*hit_invalid=*/true, /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    g_core->seal_skill(ctx, owner, target, /*effect_id=*/1279, /*attribute=*/false,
                       /*attack=*/true, counts, /*duration_rounds=*/0,
                       /*penetrable=*/false, /*source_slot=*/-1, EffectScope::ON_STAGE,
                       /*hit_invalid=*/true, /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── 881 若自身处于能力下降状态则技能先制+3 ──
// 时点 = MOVE_RIGHT（先手权判定；条件成立才加）。
EffectResult effect_881_priority_when_dropped(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[2] > 0 ? args.int_args[2] : 3;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(owner, stat) < 0) {
            ctx->ws.preemptive_level[owner] += bonus;
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

// ── 1445 吸取对手能力提升状态，吸取成功则{0}回合内对手属性技能无效，
//    若对手不处于能力提升状态则自身下{1}回合先制+{2} ──
// Args=[owner,target,seal_rounds,prio_rounds,prio_bonus]。吸取 = transfer_stat_boosts(to=self)。
EffectResult effect_1445_priority_tick(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_1445_absorb_or_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int seal_rounds = args.int_args[2] > 0 ? args.int_args[2] : 2;
    const int prio_rounds = args.int_args[3] > 0 ? args.int_args[3] : 2;
    const int prio_bonus = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int moved = g_core->transfer_stat_boosts(ctx, target, owner);
    if (moved > 0) {
        // 吸取成功 → 对手 {seal_rounds} 回合属性技能无效（封属票，走 SKILL_BAN 族）。
        const int start = ctx->round_effect_start_round(owner, seal_rounds,
                                                       EffectWindowKind::InRounds);
        ctx->rule_center_.grant_skill_ban(owner, /*source_effect_id=*/1445, target,
                                          seal_rounds, start, EffectScope::ON_STAGE);
        return EffectResult::kOk;
    }
    // 对手无提升 → 自身下 {prio_rounds} 回合先制 + {prio_bonus}。
    if (prio_bonus > 0) {
        Effect wrapper(1445, 0, owner, /*left_round=*/-1,
                       EffectArgs(std::vector<int>{owner, prio_bonus}), 
                       &effect_1445_priority_tick);
        auto ce = std::make_unique<ContinuousEffect>(
            wrapper, State::BATTLE_FIRST_MOVE_RIGHT, owner, prio_rounds, ctx->roundCount);
        ce->source_id_ = 1445;
        ce->scope_ = EffectScope::ON_STAGE;
        ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

EffectResult effect_1445_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int bonus = args.int_args[1];
    if (owner < 0 || owner > 1 || bonus <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[owner] += bonus;
    return EffectResult::kOk;
}

// ── 2273 对手选择攻击技能时此技能先制+3且{0}%令对手{1} ──
// Args=[owner,target,pct,anomaly]。先制部分挂 MOVE_RIGHT（按 roundChoice 判对手是否选攻击）；
// 异常部分同点施加（时点在对手行动前，官方"命中后"语义近似——见注释）。
EffectResult effect_2273_priority_vs_attack(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 对手本回合是否选择了攻击技能（roundChoice 已在选择期写定、ROUND_START 保留）。
    constexpr int kActionSelectSkill = 1;
    if (ctx->roundChoice[target][0] != kActionSelectSkill) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[target][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(target).skills.size())) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).skills[static_cast<std::size_t>(idx)].type
        == SkillType::Attribute) {
        return EffectResult::kOk;   // 对手选的是属性技能 → 不触发
    }
    ctx->ws.preemptive_level[owner] += 3;
    if (g_core && g_core->apply_anomaly && is_valid_abnormal_status_id(anomaly)
        && roll_percent(ctx->rng(), pct)) {
        g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

// ── 1545 消除对手所有护盾效果，消除成功则为自身附加等量的护盾值 ──
EffectResult effect_1545_steal_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ElfPet& foe = ctx->getPet(target);
    const int amount = foe.shield_bank_.total();
    if (amount <= 0) {
        return EffectResult::kOk;   // 对手无护盾 → 不触发
    }
    foe.shield_bank_.clear();
    // "为自身附加等量的护盾值"——官方未写护盾的持续/优先级 → 走魂印专属优先级、破盾前持续。
    ctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority, amount,
                                       /*source_id=*/1545, /*duration=*/0);
    return EffectResult::kOk;
}

// ── 2198 双方任意一方处于护盾状态时{0}%令对手{1} ──
EffectResult effect_2198_shield_sleep(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // "双方任意一方处于护盾状态"——护盾（shield_bank_）**不含护罩**（hood_bank_ 是独立实体）。
    if (ctx->getPet(owner).shield_bank_.total() <= 0
        && ctx->getPet(target).shield_bank_.total() <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly && is_valid_abnormal_status_id(anomaly)
        && roll_percent(ctx->rng(), pct)) {
        g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), owner);
    }
    return EffectResult::kOk;
}

// ── 405 先出手时，额外附加{0}点固定伤害 ──
EffectResult effect_405_first_move_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {
        g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

// ── 721 后出手时使对手下{0}回合攻击技能MISS ──
// Args=[owner,target,rounds]。窗口 = ROUND_START 条目（duration=rounds），载荷每回合把
// 对手的命中率压到 0（ws.hit_rate_mod 的"攻击技能 MISS"近似——官方只写攻击技能，
// 引擎的 hit_rate_mod 同时作用于属性技；v1 近似见注释）。
EffectResult effect_721_miss_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int target = args.int_args[1];
    if (target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.hit_rate_mod[target] = 0.0f;   // 对手本回合攻击必 MISS
    return EffectResult::kOk;
}

EffectResult effect_721_second_move_miss(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2] > 0 ? args.int_args[2] : 2;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 仅后出手
    }
    Effect wrapper(721, 0, owner, /*left_round=*/-1,
                   EffectArgs(std::vector<int>{owner, target}), &effect_721_miss_tick);
    auto ce = std::make_unique<ContinuousEffect>(
        wrapper, State::BATTLE_ROUND_START, owner, rounds, ctx->roundCount);
    ce->source_id_ = 721;
    ce->scope_ = EffectScope::ON_STAGE;
    ctx->register_skill_effect(State::BATTLE_ROUND_START, owner, std::move(ce));
    return EffectResult::kOk;
}

// ── 2278 若对手为狩杀之的则造成的攻击伤害提升{0}%，若对手不为狩杀之的则自身当回合
//    无法造成攻击伤害且附加等同于狩杀之的体力{1}%的百分比伤害，自身拥有狩杀之魂时
//    上述效果提升{2}% ──
// Args=[owner,target,boost_pct,hp_pct,soul_bonus]。与魂印侧狩杀之的/狩杀之魂状态联动。
EffectResult effect_2278_hunt_target_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int boost_pct = args.int_args[2];
    const int hp_pct = args.int_args[3];
    const int soul_bonus = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].soulmark_storage;
    auto sit = storage.find(kMarkSideKey);
    auto lit = storage.find(kMarkSlotKey);
    const bool has_soul = hunt_soul_of(ctx, owner) > 0;
    const int total_bonus = has_soul ? (boost_pct + soul_bonus) : boost_pct;
    const bool target_is_marked = [&]() {
        if (sit == storage.end() || lit == storage.end()) {
            return false;
        }
        return any_int_or(sit->second) == target
            && any_int_or(lit->second) == ctx->on_stage[target];
    }();

    if (target_is_marked) {
        if (total_bonus > 0) {
            ctx->ws.damage_add_pct[owner] += total_bonus;   // 狩杀之的 → 增伤
        }
        return EffectResult::kOk;
    }
    // 非狩杀之的 → 本回合无法造成攻击伤害（伤害归零）+ 附加"标记宠体力"的百分比伤害。
    ctx->ws.damage_add_pct[owner] -= 100;   // -100% = 归零（管线内钳 0）
    const int mark_slot = marked_slot_of(ctx, owner);
    if (mark_slot >= 0 && g_core && g_core->deal_pink_damage) {
        const int mark_side = any_int_or(sit->second);
        const int mark_hp = ctx->seerRobot[mark_side].elfPets[mark_slot].hp;
        int pct = has_soul ? (hp_pct + soul_bonus) : hp_pct;
        if (pct > 0) {
            g_core->deal_pink_damage(ctx, target, mark_hp * pct / 100,
                                     DamageKind::PERCENT_VALUE, owner);
        }
    }
    return EffectResult::kOk;
}

// ── 2276/2277 将对方场下对自身克制系数最高/最低且存活的一只非狩杀之的精灵标记为狩杀之的 ──
// Args=[owner,target]。克制系数走 CoreApi 的 restraint_multiplier（本系 vs 目标固有系）。
// 标记实现（highest=true 取克制最高、false 取最低）。
EffectResult mark_hunt_target(BattleContext* ctx, int owner, bool highest) {
    if (!ctx || owner < 0 || owner > 1 || !g_core || !g_core->restraint_multiplier) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;
    const int her_slot = ctx->on_stage[owner];
    const int her_elem[2] = {ctx->getPet(owner).elementalAttributes[0],
                             ctx->getPet(owner).elementalAttributes[1]};
    int best_slot = -1;
    double best_coef = highest ? -1.0 : 1e9;
    for (int slot = 0; slot < 6; ++slot) {
        const ElfPet& pet = ctx->seerRobot[foe].elfPets[slot];
        if (pet.hp <= 0 || ctx->is_vanished(foe, slot)) {
            continue;   // 存活限定
        }
        if (slot == ctx->on_stage[foe]) {
            continue;   // "场下"限定
        }
        const int foe_elem[2] = {pet.elementalAttributes[0], pet.elementalAttributes[1]};
        const double coef = g_core->restraint_multiplier(her_elem, foe_elem);
        if ((highest && coef > best_coef) || (!highest && coef < best_coef)) {
            best_coef = coef;
            best_slot = slot;
        }
    }
    if (best_slot < 0) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->seerRobot[owner].elfPets[her_slot].soulmark_storage;
    storage[kMarkSideKey] = foe;
    storage[kMarkSlotKey] = best_slot;
    return EffectResult::kOk;
}

EffectResult effect_2276_mark_highest(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    return mark_hunt_target(ctx, args.int_args[0], /*highest=*/true);
}

EffectResult effect_2277_mark_lowest(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    return mark_hunt_target(ctx, args.int_args[0], /*highest=*/false);
}

}  // namespace

// ── 效果注册（静态自注册）──────────────────────────────────────────
// ⚠️ 881 / 2273 需要 MOVE_RIGHT 时点（条件先制）——已在 skills.cpp 的 effect_register_state 补 case。

    SKILL_EFFECT(1445, effect_1445_absorb_or_priority);   // 穿林响：吸取对手提升 / 无提升则先制
    SKILL_EFFECT(2273, effect_2273_priority_vs_attack);   // 穿林响：对手选攻击→先制+3+异常
    SKILL_EFFECT(1545, effect_1545_steal_shield);         // 抿叶扬音：消除对手护盾（等量转移）
    SKILL_EFFECT(2198, effect_2198_shield_sleep);         // 抿叶扬音：任一方有护盾→睡眠
    SKILL_EFFECT(405, effect_405_first_move_fixed);       // 信步嶙峋：先出手额外固伤
    SKILL_EFFECT(721, effect_721_second_move_miss);       // 信步嶙峋：后出手→对手攻击MISS
    SKILL_EFFECT(881, effect_881_priority_when_dropped);  // 森语百兽谣：自身下降→先制+3
    SKILL_EFFECT(1279, effect_1279_reverse_then_seal);    // 森语百兽谣：反转自身下降→封对手技能
    SKILL_EFFECT(2278, effect_2278_hunt_target_damage);   // 森语百兽谣：狩杀之的增伤联动
    SKILL_EFFECT(2276, effect_2276_mark_highest);         // 神驰天苍：标记克制最高的场下精灵
    SKILL_EFFECT(2277, effect_2277_mark_lowest);          // 翎落地茫：标记克制最低的场下精灵
