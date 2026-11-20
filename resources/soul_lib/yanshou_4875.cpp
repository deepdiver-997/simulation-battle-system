// 永烬劫炎·上古炎兽 4875 魂印 —— 神惧劫炎 / 虚拟烧伤源头 / 自身异常转化
//
// 官方魂印（effect_icon 2481，4399 问答 1010874）：
//   战斗阶段结束时，若对手处于烧伤状态则获得3回合的神惧劫炎，自身已处于神惧劫炎时
//   改为令其回合数+1，若对手不处于烧伤状态则恢复自身最大体力的1/3并附加恢复量的
//   百分比伤害，然后若此伤害未减少对手体力则自身额外获得最大体力1/3的护盾；
//   对手使用攻击技能时令对手烧伤，未触发则消除对手回合类效果，然后若自身不处于烧伤
//   则自身下1次进入的异常状态转化为2回合的烧伤；
//   自身存活于出战背包时，对手免疫能力下降状态时视为处于烧伤状态；
//   神惧劫炎：回合结束后，若自身存在技能已损失PP高于对手对应位置的技能则将其已损失PP
//   调整至与自身相同;自身先出手时效果回合数-1且对手的烧伤回合数+1;自身所有技能先制+2
//   且造成的攻击伤害提升100%;最高8回合。
//
// 关键口径（2026-09-20 烧伤三精灵线）：
//   ★ "视为处于烧伤状态" = **虚拟烧伤**：插件侧查询层（include/effects/burn_perception.h），
//     引擎异常体系零改动。检测类效果（希拓/织焰魂印、破魂枪、将来的灼眼焰华条件龙威）
//     走 is_burn_effective 都查得到；**烧伤→焚烬转化**类（effect 572 等）查 is_real_burn
//     ——虚拟烧伤不触发转化（官方口径：要真正的异常才能触发转化）。
//     本魂印自己的"若对手处于烧伤状态"也是检测类 → 虚拟也算（免弱对手反哺神惧劫炎，
//     正是这条魂印的反制设计）。
//   ★ 神惧劫炎 = 在场期增益（on_stage_storage，换宠清空），剩余回合数 int。回合结束定序：
//     ①PP 调整（把**对手**对应位置已损失 PP 拉到与自身相同——"其"指对手的技能，进攻向）
//     ②先出手 → 回合数-1 且对手真实烧伤回合数+1  ③自然 -1  ④主子句（烧伤 +1/得 3，封顶 8）。
//     持续被烧时 +1/-1 相抵（稳定持有），断烧后逐回合衰减——口径待实测，记档。
//   ★ PP 调整/烧伤+1 只动**真状态**（虚拟烧伤无回合数可加）。
//   ★ 先手方判定与 FSM resolve_first_mover_id 同源（preemptive_right）。
//   ★ "下1次进入的异常状态转化为2回合烧伤"：预置 ctx->anomaly_conversion[owner][*]=烧伤
//     （内核 [6] 单跳转化）+ EVENT_ANOMALY_APPLIED 监听：落地的是烧伤即把时长改 2 回合、
//     清转化表。监听按事件 amount（落地异常 id）过滤——古早通道绕过转化落地别的异常时
//     不算"进入的异常被转化"，不消耗。
//   ★ 恢复分支的"附加恢复量的百分比伤害"走 PERCENT_VALUE（值已算好、档位仍属百分比，
//     吃护罩/免粉）；"未减少对手体力"= 这段粉伤一点没打进去（护罩全吸/免粉）→ 补魂印盾。
#include "common.h"
#include <effects/burn_perception.h>
#include "../burn_trio_runtime.h"

namespace {

// "下1次进入的异常已预置转化"旗（on_stage_storage，换宠随转化表一起失效——
// perform_switch 会清 ctx->anomaly_conversion[owner]，两侧生命周期对齐）。
constexpr int kYanshouConvArmedKey = 2483;

// "对手使用攻击技能时"已响应的回合号（on_stage_storage）——双节点去重用。
// ⚠️ 桶语义：AFTER_ACTION 族只跑**行动方**自己的条目，本方节点抓不到"对方桶"。
// 捕获点 = 本方 BATTLE_SECOND_ACTION_START（本方后手：先手对手刚行动完）+
// 本方 BATTLE_ROUND_END（本方先手：对方行动后本方没有更早的自有桶）。
constexpr int kYanshouReactedRoundKey = 2484;

constexpr int kBurnId = static_cast<int>(AbnormalStatusId::Burn);

bool yanshou_on_stage(BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->on_stage[side] >= 0
        && ctx->on_stage[side] < 6 && ctx->getPet(side).id == kYanshouPetId;
}

int first_mover_of(const BattleContext* ctx) {
    return ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
}

bool opponent_used_attack_skill(BattleContext* ctx, int side) {
    const int opp = 1 - side;
    if (ctx->roundChoice[opp][0] != 1 /*SELECT_SKILL*/) {
        return false;
    }
    const int idx = ctx->roundChoice[opp][1];
    if (idx < 0 || idx >= static_cast<int>(ctx->getPet(opp).skills.size())) {
        return false;
    }
    // "使用攻击技能"：静态技能类型判（变系别视图场景待实测）。
    return ctx->getPet(opp).skills[static_cast<std::size_t>(idx)].type != SkillType::Attribute;
}

bool already_reacted(const ElfPet& host, int round) {
    const auto it = host.on_stage_storage.find(kYanshouReactedRoundKey);
    return it != host.on_stage_storage.end() && any_int_or(it->second) == round;
}

// ── "下1次进入的异常 → 2回合烧伤"：预置转化表 + 落地监听 ──
void arm_burn_conversion(BattleContext* ctx, int owner) {
    ElfPet& host = ctx->getPet(owner);
    if (host.on_stage_storage.count(kYanshouConvArmedKey)) {
        return;   // 已预置，不重复
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (id == kBurnId) {
            continue;   // 烧伤进烧伤无需转化（自然落地的烧伤不改时长、不消耗）
        }
        ctx->anomaly_conversion[owner][id] = kBurnId;
    }
    host.on_stage_storage[kYanshouConvArmedKey] = 1;
    // 落地监听：转化后的烧伤落地（APPLIED 事件 amount = 落地异常 id）→ 改 2 回合 + 撤除。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ANOMALY_APPLIED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/true,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.amount != kBurnId) {
                return;   // 古早通道绕过转化落地别的异常：不算，转化仍预置
            }
            ElfPet& pet = c->seerRobot[owner].elfPets[c->on_stage[owner]];
            if (!pet.on_stage_storage.count(kYanshouConvArmedKey)) {
                return;   // 已被撤（换宠清转化表）：不重复改时长
            }
            pet.on_stage_storage.erase(kYanshouConvArmedKey);
            c->anomaly_conversion[owner].clear();
            c->abnormal_status_end_round[owner][kBurnId] = c->roundCount + 2;
        },
        WatcherScope::TEAM);
}

// ── 登场：接线神惧劫炎的攻击伤害 +100%（管线实时读持有状态，零上古对局零开销）──
EffectResult yanshou_enter(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !yanshou_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (ctx->plugin_storage.count(kYanshouPipelineWiredKey)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kYanshouPipelineWiredKey] = true;
    for (int owner = 0; owner < 2; ++owner) {
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                if (yanshou_shenju_rounds(c->getPet(bucket_owner)) <= 0) {
                    return;
                }
                d.addPct += 100;
                d.final = d.final * 2;
            });
    }
    return EffectResult::kOk;
}

// ── 先手权结算：神惧劫炎持有中 → 所有技能先制+2 ──
EffectResult yanshou_move_right(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !yanshou_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (yanshou_shenju_rounds(ctx->getPet(side)) > 0) {
        ctx->ws.preemptive_level[side] += 2;
    }
    return EffectResult::kOk;
}

// ── 先手方行动后：记录"本回合先出手"（神惧劫炎的先手衰减判据）──
EffectResult yanshou_first_after_action(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !yanshou_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    if (first == side && ctx->roundChoice[side][0] == 1 /*SELECT_SKILL*/) {
        yanshou_set_first_mover_round(ctx->getPet(side), ctx->roundCount);
    }
    return EffectResult::kOk;
}

// ── "对手使用攻击技能时"本体：令对手烧伤；未触发 → 消除对手回合类效果；
//     自身未烧伤 → 预置"下1次进入的异常 → 2回合烧伤"。命中与否不 gating"使用"。
void yanshou_react_to_attack(BattleContext* ctx, int side) {
    const int opp = 1 - side;
    if (!g_core || !g_core->apply_anomaly) {
        return;
    }
    const ApplyAnomalyResult r =
        g_core->apply_anomaly(ctx, opp, kBurnId, /*duration=*/-1, side);
    const bool landed = r == ApplyAnomalyResult::SUCCESS
                     || r == ApplyAnomalyResult::REPLACED_EXISTING
                     || r == ApplyAnomalyResult::DURATION_EXTENDED;
    if (!landed && g_core->break_round_effects) {
        // 未触发 → 消除对手回合类效果（真原语：查免断 + EVENT_BREAK 全链路）。
        g_core->break_round_effects(ctx, opp);
    }
    if (!is_burn_effective(ctx, side)) {
        arm_burn_conversion(ctx, side);
    }
}

// 节点①：本方为后手 —— 先手对手刚行动完（本方 BATTLE_SECOND_ACTION_START）。
EffectResult yanshou_react_as_second(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !yanshou_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (!opponent_used_attack_skill(ctx, side)) {
        return EffectResult::kOk;   // 对手没用攻击技能（属性/嗑药/被控/换宠）
    }
    ctx->getPet(side).on_stage_storage[kYanshouReactedRoundKey] = ctx->roundCount;
    yanshou_react_to_attack(ctx, side);
    return EffectResult::kOk;
}

// 节点②：本方为先手 —— 对手（后手）行动后的最早自有桶 = 本方 ROUND_END。
EffectResult yanshou_react_as_first(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !yanshou_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (first_mover_of(ctx) != side || !opponent_used_attack_skill(ctx, side)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(side);
    if (already_reacted(host, ctx->roundCount)) {
        return EffectResult::kOk;   // 节点①已响应（本方后手路径）
    }
    host.on_stage_storage[kYanshouReactedRoundKey] = ctx->roundCount;
    yanshou_react_to_attack(ctx, side);
    return EffectResult::kOk;
}

// ── 战斗阶段结束（BATTLE_ROUND_END）：神惧劫炎结算 + 主子句 ──
EffectResult yanshou_round_end(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !yanshou_on_stage(ctx, side) || !g_core) {
        return EffectResult::kOk;
    }
    const int opp = 1 - side;
    ElfPet& host = ctx->getPet(side);
    ElfPet& foe = ctx->getPet(opp);
    const bool was_active = yanshou_shenju_rounds(host) > 0;

    // ① PP 调整：自身某槽已损失 PP 高于对手对应位置 → 把**对手**的已损失拉到与自身相同。
    if (was_active) {
        const int n = std::min(host.skills.size(), foe.skills.size());
        for (std::size_t i = 0; i < n; ++i) {
            const Skills& mine = host.skills[i];
            Skills& theirs = foe.skills[i];
            if (mine.pp < 0 || theirs.pp < 0) {
                continue;   // 无限 PP 槽跳过
            }
            const int my_lost = mine.maxPP - mine.pp;
            const int their_lost = theirs.maxPP - theirs.pp;
            if (my_lost > their_lost) {
                theirs.pp = std::max(0, theirs.pp - (my_lost - their_lost));
            }
        }
        // ② 自身先出手：效果回合数-1 且对手的（真实）烧伤回合数+1。
        if (yanshou_first_mover_round(host) == ctx->roundCount
            && is_real_burn(ctx, opp)) {
            ctx->abnormal_status_end_round[opp][kBurnId] += 1;
            yanshou_set_shenju_rounds(host, yanshou_shenju_rounds(host) - 1);
        }
        // ③ 自然衰减 1 回合。
        yanshou_set_shenju_rounds(host, yanshou_shenju_rounds(host) - 1);
    }

    // ④ 主子句。
    if (is_burn_effective(ctx, opp)) {
        // 对手处于（有效）烧伤：已持有 → +1；未持有 → 获得 3 回合。封顶 8（setter 钳）。
        yanshou_set_shenju_rounds(host, was_active ? yanshou_shenju_rounds(host) + 1 : 3);
    } else if (g_core->heal_amount) {
        // 对手没烧：恢复 1/3 → 附加恢复量的百分比伤害（PERCENT_VALUE）。
        const int before = host.hp;
        g_core->heal_amount(ctx, side, resolve_pet_max_hp(host) / 3);
        const int healed = host.hp - before;
        const int foe_hp_before = foe.hp;
        if (healed > 0 && g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, side);
        }
        // 此伤害未减少对手体力（护罩全吸/免粉/零恢复都没打进）→ 额外获得 1/3 魂印盾。
        if (healed > 0 && foe.hp == foe_hp_before) {
            host.shield_bank_.add(ShieldBank::kSoulShieldPriority,
                                  resolve_pet_max_hp(host) / 3, kYanshouMarkId,
                                  /*duration=*/0);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kYanshouMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &yanshou_enter,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &yanshou_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &yanshou_first_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_ACTION_START, &yanshou_react_as_second,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &yanshou_react_as_first,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &yanshou_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
