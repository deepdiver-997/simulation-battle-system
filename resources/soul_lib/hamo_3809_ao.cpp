// 龙神哈莫 3809 魂印 —— 敖（effect_icon 1392，2026-09-30；场景回归 scenario_875_hamo_3809_ao）
//
// 官方 tips 全文（effect_icon.pet_id=[3809]，args={50,1,25,10,10,150,300,1}）：
//   哈莫雷特获得龙神之力，万龙臣服：所有属性中带龙系的精灵每回合50%的概率先制-1且对其
//   造成的攻击伤害降低25%；|满体力时自身受到的攻击伤害降低50%且造成的攻击伤害提升50%，
//   自身体力每降低10%则减伤效果降低10%，同时造成的伤害提升10%；|对手使用属性技能则自身
//   当回合免疫异常状态和能力下降状态，使用攻击技能则战斗阶段结束时随机附加给对手150-300点
//   固定伤害（boss无效）|御之龙魂BUFF说明：…（effect 1453 已实现）|战之龙魂BUFF说明：…
//   （effect 1452 已实现，见 moves_lib/hamo_3809_longhun.cpp + 场景 171）
//
// 机制（本文件；2026-09-30 用户定档口径）：
//   ★ 龙系压制（①）：**看场上属性**——读 `ws.view_elementalAttributes`（龙系=元素 15），
//     龙琰第五技等改系效果即时生效（与天敌判定读固有属性形成对照，用户点名的区分点）。
//     每回合掷一次 50%：成功 → 对手在场龙系宠 `ws.preemptive_level` **-1**（MOVE_RIGHT 桶
//     = 先制 memset 之后、比较之前的唯一点；束缚清零段不动它、必先不受影响——与束缚
//     "清先制不清必先"同构）+ 本回合哈莫对龙系对手的攻击伤害 ×75%（AMP 段乘区）。
//     50% 只门先制-1 与减伤这一对（官方一句连述）。
//   ★ 体力档（②）：steps = 已损失体力十分数（0..10）；受伤降低 = max(0, 50-10×steps)%
//     写 ws.damage_reduce_add（REDUCE_PCT 加算槽）、伤害提升 = 50+10×steps% 写
//     ws.damage_add_pct（通用增伤加算）——776/32 每回合重写套路（回合内掉血不刷新，
//     保守档；实测要求逐击读取再改管线内动态）。
//   ★ 免控+固伤（③，boss 无效 → 回调内手动门 is_boss_challenge）：
//     对手使用属性技能（EVENT_SKILL_USED.used_attribute，miss/被封也算"使用"）→ 哈莫
//     当回合免疫异常（ANOMALY, Ancient 层——语料《浅谈主动毒与魂印免控判定》把龙神哈莫
//     列进"免控判定可免疫主动毒"名单）+ 免疫能力下降（STAT_DROP）；duration=1 = 当回合
//     （grant 时 registered=当前回合，回合数推进即过期），同回合重复触发同句柄刷新。
//     对手使用攻击技能 → 本回合 ROUND_END 桶（战斗阶段结束，先于 OLD_ROUND_END 死亡时点
//     ——哈莫链复刻的落点要求）给对手 150-300 随机固定伤害（fixed_damage 确定值无浮动）。
//   ★ 龙魂双 BUFF（④⑤）不在本文件（1452/1453 已实现）。
#include "common.h"

namespace {

constexpr int kHamoPetId = 3809;
constexpr int kMarkId = 1392;
constexpr int kElementDragon = 15;   // 龙系元素 id（哈莫雷特/龙神哈莫 monsters.type=15）

// plugin_storage 段 38091xxx（grep 全库无撞；38090001 为龙魂 longhun 接线键）。
constexpr int kSuppressFlagKey = 38091001;   // +side：本回合龙系压制掷点结果（1/0）
constexpr int kAttackUsedKey = 38091002;     // +side：对手本回合用过攻击技能（值=roundCount）
constexpr int kWiredKey = 38091003;          // +side：watcher/AMP 接线标记
constexpr int kHandleAnomaly = 13921;        // ③ 免异常票句柄（同回合重复触发=刷新）
constexpr int kHandleStatDrop = 13922;       // ③ 免能力下降票句柄

bool on_stage_is_hamo(BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->on_stage[side] >= 0
        && ctx->on_stage[side] < 6 && ctx->getPet(side).id == kHamoPetId;
}

// 对手在场宠的**场上视图**属性含龙系（改系即时生效）。
bool foe_view_is_dragon(BattleContext* ctx, int side) {
    const int opp = 1 - side;
    const int (&view)[2] = ctx->ws.view_elementalAttributes[opp];
    return view[0] == kElementDragon || view[1] == kElementDragon;
}

// ── 接线（幂等）：③ 双半 watcher + ① 减伤 AMP 乘区 ─────────────────────────
void ensure_ao_wiring(BattleContext* ctx, int owner) {
    if (!ctx || ctx->plugin_storage.count(kWiredKey + owner)) {
        return;
    }
    ctx->plugin_storage.emplace(kWiredKey + owner, 1);

    // ③：对手使用技能 → 属性技给当回合免控票 / 攻击技立"本回合战斗阶段结束固伤"旗。
    // boss 无效子句 → 回调内手动门（watcher 不随节点 boss_invalid 走）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != 1 - owner || c->is_boss_challenge
                || !on_stage_is_hamo(c, owner)) {
                return;
            }
            if (ev.used_attribute) {
                // 当回合 = duration 1（grant 时 registered=当前回合，回合推进即过期）。
                // Ancient 层：主动毒一起挡（语料名单）。
                c->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                  /*anomaly_mask=*/0, /*duration_rounds=*/1, kHandleAnomaly,
                                  /*soul_immunity=*/true, EffectScope::ON_STAGE, /*counts=*/0,
                                  /*source_effect_id=*/kMarkId, ImmunityTier::Ancient);
                c->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                                  /*anomaly_mask=*/0, /*duration_rounds=*/1, kHandleStatDrop,
                                  /*soul_immunity=*/true, EffectScope::ON_STAGE, /*counts=*/0,
                                  /*source_effect_id=*/kMarkId, ImmunityTier::Ancient);
            } else {
                c->plugin_storage[kAttackUsedKey + owner] = c->roundCount;
            }
        },
        WatcherScope::TEAM);

    // ①：对龙系对手的攻击伤害 ×75%（哈莫侧 AMP 乘区；掷点旗在 plugin_storage）。
    ctx->register_default_damage_effect(
        DamagePhase::AMP, owner, DamageEffectCategory::AMP,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !on_stage_is_hamo(c, owner)) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.final <= 0) {
                return;
            }
            auto flag = c->plugin_storage.find(kSuppressFlagKey + owner);
            if (flag == c->plugin_storage.end()
                || any_int_or(flag->second) != 1) {
                return;
            }
            const int opp = 1 - owner;
            const int (&view)[2] = c->ws.view_elementalAttributes[d.defenderId];
            if (view[0] != kElementDragon && view[1] != kElementDragon) {
                return;
            }
            d.final = d.final * 75 / 100;
        });
}

// ── 节点⓪：ENTER_EXIT_STAGE（ROSTER）—— 接线入口（watcher/AMP 乘区一次注册）────
EffectResult ao_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ensure_ao_wiring(ctx, owner);
    return EffectResult::kOk;
}

// ── 节点①：MOVE_RIGHT（STAGE）—— 龙系压制掷点 + 先制-1 ────────────────────
EffectResult ao_suppress_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !on_stage_is_hamo(ctx, owner)) {
        return EffectResult::kOk;
    }
    ensure_ao_wiring(ctx, owner);
    const int hit = roll_percent(ctx->rng(), 50) ? 1 : 0;
    ctx->plugin_storage[kSuppressFlagKey + owner] = hit;
    if (hit && foe_view_is_dragon(ctx, owner)) {
        // 先制计算点（MOVE_RIGHT 桶在 memset 之后跑）：-1 修正，不触必先。
        ctx->ws.preemptive_level[1 - owner] -= 1;
    }
    return EffectResult::kOk;
}

// ── 节点②：ROUND_START（STAGE）—— 体力档 ws 重写（776/32 套路）─────────────
EffectResult ao_physique_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !on_stage_is_hamo(ctx, owner)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    int max_hp = self.numericalBase[NumericalPropertyIndex::HP];
    if (max_hp <= 0) {
        max_hp = 1;
    }
    const int lost = std::max(0, max_hp - self.hp);
    const int steps = std::min(10, lost * 10 / max_hp);
    // 受伤降低 = max(0, 50-10×steps)%：REDUCE_PCT 加算槽抢空位（ws 每回合 reset → 重写）。
    const int taken_reduce = std::max(0, 50 - 10 * steps);
    if (taken_reduce > 0) {
        int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
        for (int i = 0; i < 4; ++i) {
            if (slots[i] == 0) {
                slots[i] = taken_reduce;
                break;
            }
        }
    }
    // 伤害提升 = 50+10×steps%（通用增伤加算）。
    ctx->ws.damage_add_pct[owner] += static_cast<float>(50 + 10 * steps);
    return EffectResult::kOk;
}

// ── 节点③：FIRST/SECOND_MOVER_DEATH（STAGE，boss 无效）—— 战斗阶段结束固伤 ──
// （"战斗阶段结束"=出手流程结束→死亡检查时点桶，见文件头定稿；旗标消费制——
//   先手桶消费后后手桶自然哑火，一回合至多一次。）
EffectResult ao_phase_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || ctx->is_boss_challenge) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !on_stage_is_hamo(ctx, owner) || !g_core
        || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    auto armed = ctx->plugin_storage.find(kAttackUsedKey + owner);
    if (armed == ctx->plugin_storage.end()
        || any_int_or(armed->second) != ctx->roundCount) {
        return EffectResult::kOk;   // 对手本回合没用过攻击技能
    }
    ctx->plugin_storage.erase(armed);
    // "随机附加 150-300 点固定伤害"——fixed_damage 确定值通道（无浮动）。
    g_core->fixed_damage(ctx, 1 - owner, 150 + ctx->rng().next() % 151);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &ao_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &ao_suppress_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &ao_physique_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVER_DEATH, &ao_phase_end_tick,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true),
              SoulMarkNodeRef(State::BATTLE_SECOND_MOVER_DEATH, &ao_phase_end_tick,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE,
                              /*boss_invalid=*/true)});
