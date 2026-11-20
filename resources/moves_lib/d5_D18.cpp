// D 组批产 · D18（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   百罗鬼帝 4444 第五技 蔑天行·戾杀 38384 缺失效果五支：
//     726  下{0}回合若对手先出手，则令对手当回合使用的攻击技能无效（切片 [2]）
//     820  未击败对手则下{0}回合自身技能先制+{1}（切片 [2,1]）
//     1843 造成的攻击伤害若高于{0}则{1}%令对手{2}（切片 [250,100,6=害怕]）
//     2469 消耗自身的鬼祰，每消耗1道自身恢复最大体力的1/4与所有技能1点PP值，
//          达到4道时为双方进入四杀鬼囹（0 参）
//     2472 对对方所有精灵附加4点固定伤害，然后对手每有一只精灵未受到此伤害，
//          本场战斗此伤害额外乘4，最高不超过自身的最大体力（0 参）
//
// 实现要点：
//   · 726：NextRounds(MOVE_RIGHT, 2) 每回合 tick → rule_center_.grant_seal
//     (SEAL_ATTACK, counts=1, rounds=1, condition=「user 是当回合先手方」)。
//     条件盔求值在 notify（对手真正用技能时，先手权已定）→ 不白挂不残留（854 同款）；
//     counts=1 = 每回合至多无效其一次攻击；penetrable=false（条件盔既有分类）。
//   · 820：AFTER_ACTION 桥（对手存活）→ NextRounds(FIRST_MOVE_RIGHT, 2) tick
//     preemptive_level += 1（1880/D13 同落点）。
//   · 1843：AFTER_ACTION 桥读裸伤视图 resolvedDamage（final > 250 且 attackerId=self）
//     → apply_anomaly_checked(害怕6, 100%)（1679 同款时点：免死检测之后）。
//   · 2469：消耗 = bailuo::kind_count 全清（lose_kinds，魉→魑反序）；每道 heal(max/4)
//     + pp_restore(全技能+1)；n>=4 → 四杀鬼囹标记（键 44442000，双方共用战场级）。
//     ⚠️ 四杀鬼囹的**状态行为**语料缺失（idx=73 未覆盖），本批只落"进入"标记，
//     行为待官方数据补齐（finish 备注待拍板）。
//   · 2472：炸背包——在场宠走 deal_pink_damage(FIXED)（吃护罩/抗性管线），场下宠
//     直写 hp（背包无盾/罩数据，死宠/消逝不计入"受到"）；未受到的每只使
//     battle 级倍率 ×4（plugin_storage 44442000+side，跨回合持久）；单次伤害
//     钳自身最大体力。⚠️ 场下炸伤绕过伤害管线是当前无场下粉伤原语的保守实现（见 finish 备注）。

#include "common.h"
#include "../bailuo_runtime.h"

#include <any>
#include <cstdio>
#include <memory>
#include <vector>

namespace {

constexpr int k820PrioBonus = 1;
constexpr int kSishGuiYinKey = 44442000;   // 四杀鬼囹战场标记（双方共用，0/1）
constexpr int k2472MulBase = 44442100;     // + side：2472 战斗级伤害倍率（×4 累乘）

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

// ── 726 下{n}回合若对手先出手 → 无效其当回合攻击技能 ───────────────────────────
EffectResult effect_726_round_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    // 条件在 notify 时求值（先手权已定）：user（被封方）== 当回合先手方。
    auto condition = [opp](BattleContext* c, int user, int, bool) {
        if (!c) {
            return false;
        }
        const int first = c->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
        return user == opp && user == first;
    };
    ctx->rule_center_.grant_seal(
        /*source_owner=*/self, /*source_slot=*/-1, /*source_effect_id=*/726,
        /*target=*/opp, SealKind::SEAL_ATTACK,
        /*counts=*/1, /*rounds=*/1,
        /*penetrable=*/false, /*scope=*/EffectScope::ON_STAGE,
        /*condition=*/condition, /*source_valid_id=*/0);
    std::fprintf(stderr, "[726dbg] seal granted rc=%d size=%d\n", ctx->roundCount,
                 static_cast<int>(ctx->rule_center_.size()));
    return EffectResult::kOk;
}

EffectResult effect_726_arm_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 726, rounds,
                                {self, opp}, &effect_726_round_tick);
    return EffectResult::kOk;
}

// ── 820 未击败 → 下{n}回合先制+{b} ─────────────────────────────────────────────
EffectResult effect_820_prio_window_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int bonus = args.int_args[1];
    if (self < 0 || self > 1 || bonus == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[self] += bonus;
    return EffectResult::kOk;
}

EffectResult effect_820_undeathed_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || bonus <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 不给
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 820, rounds,
                                {self, bonus}, &effect_820_prio_window_tick);
    return EffectResult::kOk;
}

// ── 1843 攻击伤害 > 阈值 → 概率令对手异常 ──────────────────────────────────────
// 伤害在 ATTACK_DAMAGE 才可知 → AFTER_ACTION 桥读 resolvedDamage（1679 同款时点）。
EffectResult effect_1843_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int threshold = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || chance < 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    const DamageSnapshot& dmg = ctx->resolvedDamage;
    if (dmg.attackerId != self || dmg.defenderId != opp || dmg.final <= threshold) {
        return EffectResult::kOk;   // 没打出高于阈值的攻击伤害
    }
    (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(),
                                        self, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

EffectResult effect_1843_damage_threshold_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    register_after_action_bridge(ctx, self, 1843,
                                 {self, opp, args.int_args[2], args.int_args[3],
                                  args.int_args[4]},
                                 &effect_1843_after_action);
    return EffectResult::kOk;
}

// ── 2469 消耗鬼祰：每道 max/4 回血 + 全技能 +1 PP；4 道 → 四杀鬼囹标记 ───────────
// 回复落在 OLD_ROUND_END_1 一次性节点：本技使用扣的 PP 在效果时点之后才扣，
// SKILL_EFFECT 处直接 pp_restore 会被"只补不削"跳过（满 PP 槽不补）。
EffectResult effect_2469_round_end_refund(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->heal_amount
        || !g_core->pp_restore) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int n = args.int_args[1];
    if (self < 0 || self > 1 || n <= 0) {
        return EffectResult::kOk;
    }
    const int self_max =
        ctx->seerRobot[self].elfPets[ctx->on_stage[self]].numericalBase[NumericalPropertyIndex::HP];
    for (int i = 0; i < n; ++i) {
        (void)g_core->heal_amount(ctx, self, self_max / 4);
        (void)g_core->pp_restore(ctx, self, /*amount=*/1, /*only_empty=*/false);
    }
    return EffectResult::kOk;
}

EffectResult effect_2469_consume_guixiao(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    const int n = bailuo::kind_count(ctx, self);
    if (n <= 0) {
        return EffectResult::kOk;   // 无鬼祰可消耗
    }
    bailuo::lose_kinds(ctx, self, n);
    if (n >= 4) {
        ctx->plugin_storage[kSishGuiYinKey] = 1;   // 四杀鬼囹（行为待语料，见文件头 ⚠️）
    }
    register_once_this_round(ctx, State::BATTLE_OLD_ROUND_END_1, self, 2469, {self, n},
                             &effect_2469_round_end_refund);
    return EffectResult::kOk;
}

// ── 2472 炸背包 ────────────────────────────────────────────────────────────────
EffectResult effect_2472_roster_bomb(BattleContext* ctx, const EffectArgs& args) {
    // 0 参效果：base 固定 4 点（"对对方所有精灵附加4点固定伤害"），args 只带 [owner, opp]。
    constexpr int kBase = 4;
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int base = kBase;
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    int mult = storage_int(ctx, k2472MulBase + self);
    if (mult <= 0) {
        mult = 1;   // 首次：×1
    }
    const int self_max =
        ctx->seerRobot[self].elfPets[ctx->on_stage[self]].numericalBase[NumericalPropertyIndex::HP];
    int dmg = base * mult;
    if (dmg > self_max) {
        dmg = self_max;   // 最高不超过自身的最大体力
    }
    int untouched = 0;
    const int on_stage = ctx->on_stage[opp];
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[opp].elfPets[slot];
        if (ctx->is_vanished(opp, slot) || pet.hp <= 0) {
            ++untouched;   // 消逝/已阵亡：未受到此伤害
            continue;
        }
        if (slot == on_stage) {
            (void)g_core->deal_pink_damage(ctx, opp, dmg, DamageKind::FIXED, self);
            if (pet.hp <= 0) {
                ++untouched;   // 在场但被护罩/臣服等拦下 → 视为未受到
            }
            continue;
        }
        pet.hp = pet.hp > dmg ? pet.hp - dmg : 0;   // 背包直写（无盾/罩数据，保守实现）
    }
    // 每有一只未受到 → 倍率 ×4（战斗级持久；封顶防溢出）
    for (int i = 0; i < untouched && mult <= (1 << 20); ++i) {
        mult *= 4;
    }
    ctx->plugin_storage[k2472MulBase + self] = mult;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(726, effect_726_arm_window);
SKILL_EFFECT(820, effect_820_undeathed_bridge);
SKILL_EFFECT(1843, effect_1843_damage_threshold_anomaly);
SKILL_EFFECT(2469, effect_2469_consume_guixiao);
SKILL_EFFECT(2472, effect_2472_roster_bomb);
