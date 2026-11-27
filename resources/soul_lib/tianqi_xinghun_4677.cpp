// 星盘族（星辰万象·天启星魂 4677：星启 + 条件免疫 + 星盘 12 刻 + 覆宙星回）
//
// 官方魂印（4399 图鉴 996676，机制解析=语料 idx=300）：
//   ① 天启星魂计算攻击伤害时双攻值视为二者之和，特殊攻击先制额外+1，
//      物理攻击使用后吸取对手双防值总和30%的体力
//   ② 每次登场时进行星启：解除自身所处的异常状态，然后若自身攻击值高于特攻则为自身
//      附加2回合星赐，若低于则改为附加星哲
//   ③ 处于星赐时免疫疲惫，处于星哲时免疫害怕，均处于时免疫控制类异常状态
//   ④ 自身异常状态结束的当回合结束后转动星盘1刻，转动成功下回合结束后自身再次进行星启；
//      星盘转动12个刻度后直到刻度回返至0不会再次转动，期间自身免疫受到的攻击且每回合
//      进行一次额外行动触发覆宙星回：附加120点神秘•轮回系伤害并令星盘回返1个刻度
//   ⑤ 转动星盘：当回合自身受到的伤害额外减少99%(包括固定伤害、百分比伤害)
//      且回合结束时令对手随机1项技能PP值归零
//
// 关键口径（用户 2026-09-20 拍板，见 EVENT_ANOMALY_EXPIRED 注释）：
//   ★ 星盘**只认"剩余回合数归零"的自然到期**——衍化（转化，不管转出是不是同一异常）、
//     转移、列奥尼达神谕冻结回合数都转不动星盘；宙始星刻 2207 是"加速消耗"（压回合数），
//     压到 0 仍走自然到期 → 正常转；被神谕冻结时压不动 → 转不了。解除异常能无视神谕，
//     但解除后异常已不在身上，同样不发到期事件（不会误转星盘）。
//   ★ 时序：到期发生在回合扣减点（roundCount=N，第 N+1 回合的收尾段）→ 立即转 1 刻；
//     "转动星盘"的 ⑤（受伤-99% + 回合结束PP归零）与"下回合结束后再次星启"都落在
//     **下一回合**（roundCount+1）：shield_round / re_star_at 都记 roundCount+1。
//   ★ "期间自身免疫受到的攻击"= 满盘期（dial==12 到回返至 0）：红伤免疫票 +
//     每回合声明一次额外行动（覆宙星回：120 点神秘·轮回系固定伤害 + dial-1）。
//   ★ ①的双攻之和是**伤害计算视图**（ws.battle_attrs，登场+每回合重断言，每回合被
//     sync_workspace_from_on_stage 重基所以必须重写）——不写面板，星启的攻特攻比较、
//     其他"攻击值高于"类效果仍读原始面板。
//   ★ 星赐/星哲"2回合"用 apply_anomaly 的引擎口径（施加时 roundCount 起算），
//     登场早于首回合 → 恰好覆盖第 1、2 回合，第 3 回合收尾段自然到期 → 转盘。
#include "common.h"
#include "../tianqi_runtime.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4677 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上
//    （2026-09-27 修复；effect_id 取自 unity 库 effect_icon 1879 行 [4677]→2272）。
constexpr int kTianqiMarkId = 2272;
constexpr int kFifthSkillId = 28810;   // 魂·天忤太虚（2210 转化判定用）
constexpr int kStarBlessing = static_cast<int>(AbnormalStatusId::StarBlessing);   // 星赐 33
constexpr int kStarWisdom = static_cast<int>(AbnormalStatusId::StarWisdom);       // 星哲 34
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);             // 疲惫 7
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);                   // 害怕 6
constexpr int kMysteryCycleElement = 123;  // 神秘·轮回（skill_types 123；28810 自身系别）
constexpr int kFullDial = 12;
constexpr int kCoverageAll = -1;           // 免疫票全时点覆盖（原 ~0ULL 经 int 收窄恒为 -1，显式落值消 -Woverflow）

int host_slot(BattleContext* ctx, int side) {
    const int slot = ctx->find_pet_with_soulmark(side, kTianqiMarkId);
    return slot;
}

// ── 星启（②）：解除自身异常 → 按攻特攻比较附加星赐/星哲（2回合）──
// 登场时点调用（ENTER_EXIT_STAGE early 节点：战斗开始 + 每次切换上场都会跑）。
void star_awaken(BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    const int slot = ctx->on_stage[side];
    ElfPet& host = ctx->seerRobot[side].elfPets[slot];
    if (host.soulMark.id != kTianqiMarkId || host.hp <= 0) {
        return;
    }
    // ① 解除自身所处异常（效果解除路径——不发生自然到期，不转星盘）。
    if (g_core && g_core->dispel_active_anomalies) {
        g_core->dispel_active_anomalies(ctx, side);
    }
    // ② 攻击值 vs 特攻值（**原始面板**——双攻视图只作用于伤害计算）。
    const int atk = host.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK];
    const int spatk = host.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK];
    if (atk > spatk && g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, side, kStarBlessing, /*duration=*/2, /*actor=*/side);
    } else if (atk < spatk && g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, side, kStarWisdom, /*duration=*/2, /*actor=*/side);
    }
    // 相等：魂印原文只写"高于/低于"，两不满足 → 不附加（字面口径）。
}

// ── ③ 条件免疫票：静态 mask 表达不了"由当前异常动态决定"，走 RuleCenter 条件回调 ──
void grant_star_conditional_immunity(BattleContext* ctx, int side) {
    auto cond = [side](const BattleContext* c, int status_id) -> bool {
        if (!c || side < 0 || side > 1 || !g_core || !g_core->has_active_anomaly) {
            return false;
        }
        const bool ci = g_core->has_active_anomaly(const_cast<BattleContext*>(c), side, kStarBlessing);
        const bool zhe = g_core->has_active_anomaly(const_cast<BattleContext*>(c), side, kStarWisdom);
        if (status_id == kFatigue) {
            return ci;                       // 星赐 → 免疫疲惫
        }
        if (status_id == kFear) {
            return zhe;                      // 星哲 → 免疫害怕
        }
        if (is_control_abnormal_status(static_cast<AbnormalStatusId>(status_id))) {
            return ci && zhe;                // 均处于 → 免疫控制类
        }
        return false;
    };
    // anomaly_mask=0（全异常，由条件回调收口）+ window（counts=0 不随施加消耗）。
    ctx->rule_center_.grant_immune(side, static_cast<int>(ImmunityType::ANOMALY),
                                   kCoverageAll, /*anomaly_mask=*/0,
                                   /*duration_rounds=*/0, ctx->roundCount, /*source_id=*/0,
                                   /*soul=*/false, EffectScope::ON_STAGE, /*source_slot=*/-1,
                                   /*counts=*/0, /*source_effect_id=*/kTianqiMarkId,
                                   static_cast<int>(ImmunityTier::Modern), std::move(cond));
}

// ── ① 双攻之和伤害视图（不写面板；每回合被 sync 重基 → ROUND_START 重断言）──
void assert_dual_attack_view(BattleContext* ctx, int side) {
    const int slot = host_slot(ctx, side);
    if (slot < 0 || ctx->on_stage[side] != slot) {
        return;   // 只有魂印宿主在场时才改本方视图
    }
    ElfPet& host = ctx->seerRobot[side].elfPets[slot];
    const int sum = host.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK]
                  + host.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK];
    ctx->ws.battle_attrs[side][NumericalPropertyIndex::PHYSICAL_ATTACK] = sum;
    ctx->ws.battle_attrs[side][NumericalPropertyIndex::SPECIAL_ATTACK] = sum;
}

// ── 满盘/回返期"免疫受到的攻击"票的授/刷/撤 ──
// ⚠️ 官方口径（用户 2026-09-20）：免疫攻击**可以被穿** → 走封属族（SEAL_ATTACK，
// penetrable=true，699"无视攻击免疫"凭证可穿），不是伤害管线 BLOCK 的 DAMAGE 免疫票。
// 授在**对手**身上（seal 的 target=被封方=技能使用方），source=本方（ON_STAGE 生命周期锚：
// 她下场即作废）。回返期间（12→0 整段）免疫持续；dial 回 0 即撤。
void sync_attack_immunity(BattleContext* ctx, int side, TianqiRuntime& state) {
    const bool active = state.returning || state.dial >= kFullDial;
    if (active) {
        // 覆盖键刷新：同 source_effect_id 重授 → 刷新窗口（回合型 duration=1 逐回合续）
        state.atk_immune_handle = g_core
            ? g_core->seal_skill(ctx, /*source=*/side, /*target=*/1 - side,
                                 /*effect_id=*/kTianqiMarkId, /*attribute=*/false,
                                 /*attack=*/true, /*count=*/0, /*duration_rounds=*/1,
                                 /*penetrable=*/true, /*source_slot=*/-1,
                                 EffectScope::ON_STAGE, /*hit_invalid=*/false,
                                 /*chance_pct=*/100, /*consumed_when_pierced=*/false)
            : 0;
    } else if (state.atk_immune_handle != 0) {
        ctx->rule_center_.revoke(state.atk_immune_handle);
        state.atk_immune_handle = 0;
    }
}

// ── 对手随机 1 项技能 PP 归零（⑤；无限 PP（pp<0）不选）──
void zero_random_pp(BattleContext* ctx, int victim) {
    ElfPet& pet = ctx->seerRobot[victim].elfPets[ctx->on_stage[victim]];
    std::vector<int> candidates;
    for (std::size_t i = 0; i < pet.skills.size(); ++i) {
        if (pet.skills[i].pp > 0) {
            candidates.push_back(static_cast<int>(i));
        }
    }
    if (candidates.empty()) {
        return;
    }
    pet.skills[candidates[ctx->rng().next() % candidates.size()]].pp = 0;
}

// ── 到期事件监听（④）：自然到期 → 转盘（满盘不再转）──
void on_anomaly_expired(BattleContext* ctx, int side, const BattleEvent& ev) {
    if (ev.target != side) {
        return;
    }
    TianqiRuntime& state = tianqi_rt(ctx->plugin_storage, side);
    if (state.dial >= kFullDial || state.returning) {
        return;   // 满 12 刻后直到回返至 0 不会再转（回返期含 dial 11..0）
    }
    if (state.last_turn_round == ctx->roundCount) {
        return;   // 官方口径：一回合最多转一次（多个异常同回合结束只算一次）
    }
    state.last_turn_round = ctx->roundCount;
    ++state.dial;
    tianqi_publish(ctx->plugin_storage, side, state.dial, state.returning);
    // 转盘效果（⑤）与再星启都落在下一回合（tick 时 roundCount 尚未 advance）。
    state.shield_round = ctx->roundCount + 1;
    state.re_star_at = ctx->roundCount + 1;
    if (state.dial >= kFullDial) {
        state.returning = true;   // 进入回返期间（2208/2209 翻倍档 + 满盘机制开闸）
    }
    tianqi_publish(ctx->plugin_storage, side, state.dial, state.returning);
    sync_attack_immunity(ctx, side, state);
}

// ── 节点①：ENTER_EXIT_STAGE（early, STAGE）—— 每次登场：星启 + 票 + 监听 + 粉伤管线 ──
EffectResult tianqi_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    TianqiRuntime& state = tianqi_rt(ctx->plugin_storage, side);
    star_awaken(ctx, side);
    grant_star_conditional_immunity(ctx, side);
    assert_dual_attack_view(ctx, side);
    sync_attack_immunity(ctx, side, state);

    tianqi_publish(ctx->plugin_storage, side, state.dial, state.returning);
    // 到期监听只挂一次（TEAM 域：跨切换保留）。
    if (state.watcher_id == 0) {
        state.watcher_id = ctx->event_center_.register_watcher(
            EventType::EVENT_ANOMALY_EXPIRED, side, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [side](BattleContext* c, const BattleEvent& ev) {
                on_anomaly_expired(c, side, ev);
            },
            WatcherScope::TEAM);
    }

    // 转盘 shield 的粉伤半边（红伤半边走 damage_reduce_mul，见 ROUND_START 节点）。
    // 管线条目按 ctx 注册一次；回调动态判 shield_round，非 shield 回合直接早退。
    if (!state.pink_registered) {
        state.pink_registered = true;
        ctx->register_pink_damage_effect(
            PinkDamagePhase::REDUCE_EXTRA, side,
            [side](BattleContext* c, int bucket_owner) {
                if (!c || bucket_owner != side || c->resolvedPink.target != side) {
                    return;
                }
                if (tianqi_rt(c->plugin_storage, side).shield_round != c->roundCount || c->resolvedPink.final <= 0) {
                    return;
                }
                c->resolvedPink.final -= c->resolvedPink.final * 99 / 100;
            },
            EffectScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 节点②：ROUND_START（STAGE）—— 视图重断言 + shield 红伤半边 + 满盘声明额外行动 ──
EffectResult tianqi_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    TianqiRuntime& state = tianqi_rt(ctx->plugin_storage, side);
    assert_dual_attack_view(ctx, side);
    // 转盘 shield 红伤半边：-99%（加算槽钳 ±100，单独一条 99 合规）。
    if (state.shield_round == ctx->roundCount && ctx->on_stage[side] == host_slot(ctx, side)) {
        ctx->ws.damage_reduce_mul[side][3] = 99;
    }
    // 满盘/回返期：每回合声明一次额外行动（覆宙星回由 EXTRA_ACTION 桶节点结算），
    // 并逐回合续"攻击无效"票。
    if ((state.returning || state.dial >= kFullDial)
        && ctx->on_stage[side] == host_slot(ctx, side)) {
        ctx->declare_extra_action(side);
        sync_attack_immunity(ctx, side, state);
    }
    return EffectResult::kOk;
}

// ── 节点③：MOVE_RIGHT（STAGE）—— 特殊攻击先制+1（按本次选择，1263 同款时点）──
EffectResult tianqi_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[side][1];
    ElfPet& host = ctx->getPet(side);
    if (idx < 0 || idx >= static_cast<int>(host.skills.size())) {
        return EffectResult::kOk;
    }
    const Skills& chosen = host.skills[static_cast<std::size_t>(idx)];
    // 官方口径（语料 idx=300）：2210 转化判定**早于**封效果/先制计算——五技按星赐/星哲
    // 转化成物理/特殊后，同样吃"特殊攻击先制+1"。这里在先制时点直接按条件判转化态
    // （不依赖 SKILL_EFFECT 时点的视图写入——那会被封效果跳过，而转化判定不受封影响）。
    SkillType effective = chosen.type;
    if (chosen.id == kFifthSkillId && g_core && g_core->has_active_anomaly) {
        const bool ci = g_core->has_active_anomaly(ctx, side, kStarBlessing);
        const bool zhe = g_core->has_active_anomaly(ctx, side, kStarWisdom);
        if (ci && !zhe) {
            effective = SkillType::Physical;
        } else if (zhe && !ci) {
            effective = SkillType::Special;
        }
    }
    if (effective == SkillType::Special) {
        ctx->ws.preemptive_level[side] += 1;
    }
    return EffectResult::kOk;
}

// ── 节点④：ATTACK_DAMAGE（STAGE）—— 物理攻击使用后吸取对手双防和 30% ──
// 桶只跑 mover 侧 → 天然只在"她自己攻击"时结算。物理判据读 skill_type_view
// （28810 转化成物理后也吃本条，与官方"转化为物理攻击"一致）。
EffectResult tianqi_physical_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    if (ctx->ws.skill_exec_result[side] == SkillExecResult::SKILL_INVALID) {
        return EffectResult::kOk;   // 未命中不结算
    }
    if (ctx->ws.skill_type_view[side] != static_cast<int>(SkillType::Physical)) {
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->seerRobot[1 - side].elfPets[ctx->on_stage[1 - side]];
    if (opp.hp <= 0) {
        return EffectResult::kOk;
    }
    int amount = (opp.numericalProperties[NumericalPropertyIndex::DEFENSE]
                  + opp.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE])
                 * 30 / 100;
    if (amount > opp.hp) {
        amount = opp.hp;
    }
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    opp.hp -= amount;
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, side, amount);   // 吸取 = 回自身（吃封回血/恢复修正）
    }
    return EffectResult::kOk;
}

// ── 节点⑤：ROUND_END（STAGE）—— 转盘次回合收尾：PP归零 + 再次星启 ──
EffectResult tianqi_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    TianqiRuntime& state = tianqi_rt(ctx->plugin_storage, side);
    if (state.re_star_at != ctx->roundCount) {
        return EffectResult::kOk;
    }
    state.re_star_at = -1;
    zero_random_pp(ctx, 1 - side);
    star_awaken(ctx, side);
    return EffectResult::kOk;
}

// ── 节点⑥：EXTRA_ACTION（STAGE）—— 覆宙星回：120 点神秘·轮回系伤害 + 回返 1 刻 ──
// 能进本桶说明本方声明过额外行动（FSM consume 把门），这里只管结算。
void fuzhou_extra(BattleContext* ctx, int side) {
    TianqiRuntime& state = tianqi_rt(ctx->plugin_storage, side);
    if (!state.returning && state.dial < kFullDial) {
        return;
    }
    if (g_core && g_core->deal_attribute_damage) {
        const int element[2] = {kMysteryCycleElement, 0};
        g_core->deal_attribute_damage(ctx, 1 - side, /*points=*/120, element, side);
    }
    if (state.dial > 0) {
        --state.dial;   // 回返 1 刻
    }
    if (state.dial == 0) {
        state.returning = false;   // 回返至 0 → 恢复正常转动 + 撤"攻击无效"票
    }
    tianqi_publish(ctx->plugin_storage, side, state.dial, state.returning);
    sync_attack_immunity(ctx, side, state);
}

EffectResult tianqi_extra_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    fuzhou_extra(ctx, side);
    return EffectResult::kOk;
}

EffectResult tianqi_extra_second(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    fuzhou_extra(ctx, side);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kTianqiMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &tianqi_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &tianqi_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &tianqi_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_ATTACK_DAMAGE, &tianqi_physical_drain,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_ATTACK_DAMAGE, &tianqi_physical_drain,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &tianqi_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_EXTRA_ACTION, &tianqi_extra_first,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_EXTRA_ACTION, &tianqi_extra_second,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
