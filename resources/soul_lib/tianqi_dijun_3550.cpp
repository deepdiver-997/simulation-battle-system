// 天启帝君 3550（effect_icon 1128，2018 年费）—— 登场场下调剂 + 老魂免 + 阵亡数族
//
// 官方 tips 全文（effect_icon.pet_id=[3550]；图鉴 816252 同文）：
//   [帝君之罚] 首次登场时直接扣除对手所有不在场精灵1/3的最大体力，之后每次登场时触发
//             效果削弱到1/9的最大体力（帝君之罚效果使对手精灵死亡时令其残留1点体力，
//             boss无效）
//   [帝君之赐] 首次登场时恢复己方所有不在场精灵1/3的最大体力，之后每次登场时触发效果
//             削弱到1/9的最大体力（boss有效）
//   [帝君之援] 天启帝君在出战背包时，己方精灵（包括天启帝君）登场时若为满体力则当回合
//             所有技能先制+1（boss有效）
//   [帝君之御] 天生免疫所有固定伤害、百分比伤害以及异常状态，且自身在战斗中将固定伤害
//             和百分比伤害始终抗性视为100%（boss无效）
//   [帝君之阵] 己方每有一只阵亡精灵则自身受到的伤害降低10%，造成的伤害提高10%，天启
//             帝君为最后一只存活精灵时自身回合类效果无法被消除；对手每有一只不在场存活
//             精灵则自身使用技能后恢复1/10最大体力并造成等量百分比伤害，对手为最后一只
//             存活精灵时使对手无法通过自身技能恢复体力（自身受到的伤害降低和造成的伤害
//             提高效果boss有效，其他效果boss无效）
//   [帝君之殒] 天启帝君被击败时，30%概率使对手随机一只不在场精灵死亡（boss无效）
//
// 机制（2026-09-20，本文件）：
//   · 登场计数：EVENT_ENTER_STAGE 只在换宠时发（部署不发）→ 部署的"首次登场"由
//     ENTER_EXIT_STAGE 节点兜底（battle_start_done 幂等），1263 同款。
//   · 罚/赐都只作用于**不在场**（场下）且存活、未消逝的精灵；罚的削减**钳底 1**
//     （"致死时残留1点"——罚自己打出的击杀永远留 1 血，不需要死亡漏斗）。
//     场下扣血前查武心婵保护门 off_field_stats_protected（咤克斯连锁先例）。
//   · 御：① 固定/百分比抗性视为 100%（2026-11-22 迁 ws boost 层 per-slot 增量，每回合
//     ROUND_START 重写——SP④"无视抗性提升"消费读 base 时帝君回落本体；无④时行为不变）
//     ② PINK_DAMAGE + ANOMALY 全 mask 免疫票（ROSTER、永久、Ancient 层——语料
//     《浅谈主动毒与魂印免控判定》把天启帝君列进"老魂免"名单，Ancient 层连主动毒一起挡）。
//   · 阵 ±10%：ws 加算通道（damage_add_pct / damage_reduce_add）+ ROUND_START 节点
//     每回合重写（ws 每回合 reset → 776/32 套路，切换作废免费获得）；阵亡数用
//     count_dead(ALL)（battle_primitives.h 注释预留的"帝君=ALL"口径）。
//   · 阵 免断/封回血：EVENT_DEATH watcher 重判"最后一只存活"动态授/撤票
//     （grant_immunity 固定句柄 re-grant / revoke_immunity）。
//   · 殒：EVENT_DEATH（target=帝君自己）→ 30% → 对手随机一只不在场存活精灵
//     hp=0 后走 defeat_pet 死亡漏斗（场下真2命复活照常响应，圣光灵神口径）。
//   · 援：ENTER_STAGE watcher 给**任何满体力登场的己方宠**打戳（on_stage_storage，
//     引擎换宠自动清 = "本次上场"生命周期）；MOVE_RIGHT 节点读戳 → preemptive+1。
//     部署宠由 arm 节点补戳（roundCount==0 幂等）。
//   · 生命周期：ROSTER 节点随宿主阵亡停刷新（更新器兜底）；御的票在宿主消逝时由
//     EVENT_VANISH 推送撤销（武心婵同款）。
#include "common.h"

#include "../tianqi_dijun_runtime.h"

namespace {

constexpr int kDijunMarkId = 1128;

// 免疫票固定句柄（grant source_id >0 = 复用更新；arm 每次重授同句柄 → 幂等）
constexpr int kHandleYuPink = 11281;    // 御：免固定/百分比伤害
constexpr int kHandleYuAnomaly = 11282; // 御：免异常（全 mask）
constexpr int kHandleZhenBreak = 11283; // 阵：最后存活 → 回合类效果免消除
constexpr int kHandleZhenHealBlock = 11284; // 阵：对手最后存活 → 封回血

// moves_lib/common.h 的同名助手在另一个 .so 里，这边本地一份（签名同义）。
inline bool used_skill_this_round_local(BattleContext* ctx, int owner) {
    constexpr int kActionSelectSkill = 1;   // = BattleFsm::ActionType::SELECT_SKILL
    return ctx && owner >= 0 && owner <= 1
        && ctx->roundChoice[owner][0] == kActionSelectSkill;
}

// 某方当前存活精灵数（6 槽，剔消逝；阵亡 hp<=0 不算存活）。
inline int alive_count(BattleContext* ctx, int side) {
    int n = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (!ctx->is_vanished(side, slot) && ctx->seerRobot[side].elfPets[slot].hp > 0) {
            ++n;
        }
    }
    return n;
}

// ── 罚/赐 主体（entry_count 已 ++，削弱档按次数与 1308 标志定）──────────────
void dijun_punish_bless(BattleContext* ctx, int owner, int host_slot) {
    ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    TianqiDijunRuntime& rt = dijun_rt(host.soulmark_storage);
    const bool no_decay = rt.next_entry_no_decay;
    rt.next_entry_no_decay = false;   // "下1次登场"一次性
    const int denom = (rt.entry_count <= 1 || no_decay) ? 3 : 9;

    // 罚（boss无效，PvP 无 boss 不建模）：对手所有不在场存活精灵 -maxhp/denom，钳底 1。
    const int opp = 1 - owner;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[opp]) {
            continue;   // 不在场限定
        }
        ElfPet& pet = ctx->seerRobot[opp].elfPets[slot];
        if (pet.hp <= 0 || ctx->is_vanished(opp, slot)) {
            continue;
        }
        if (ctx->off_field_stats_protected(opp, slot)) {
            continue;   // 武心婵 4500 场下保护门（体力不减）
        }
        const int dmg = resolve_pet_max_hp(pet) / denom;
        if (dmg <= 0) {
            continue;
        }
        pet.hp = std::max(1, pet.hp - dmg);   // "致死时残留1点体力"
    }

    // 赐（boss有效）：己方所有不在场存活精灵 +maxhp/denom（钳上限；保护门只挡"减少"，
    // 恢复不受它管）。场下恢复无原语 → 直写（同 4677 星盘侧口径）。
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[owner]) {
            continue;
        }
        ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        if (pet.hp <= 0 || ctx->is_vanished(owner, slot)) {
            continue;
        }
        const int amount = resolve_pet_max_hp(pet) / denom;
        if (amount <= 0) {
            continue;
        }
        pet.hp = std::min(resolve_pet_max_hp(pet), pet.hp + amount);
    }
}

// ── 节点①：ENTER_EXIT_STAGE（ROSTER）—— arm 总入口 ─────────────────────
// 幂等责任：御面板/免疫票（重授同句柄）、watcher（flag）、阵场（watcher 动态）、
// 部署罚赐（battle_start_done）、部署援戳（deploy_stamped）。
EffectResult soulmark_1128_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int host_slot = ctx->find_mark_in_roster(owner, kDijunMarkId);
    if (host_slot < 0) {
        return EffectResult::kOk;   // 已消逝：不在任何空间
    }
    ElfPet& host = ctx->seerRobot[owner].elfPets[host_slot];
    TianqiDijunRuntime& rt = dijun_rt(host.soulmark_storage);

    // 御：免固定/百分比伤害 + 免异常（全 mask）。老魂免 → Ancient 层（连主动毒一起挡）。
    // （抗性 100% 的两槽改写已迁到战前魂印补丁，见文件尾 SOUL_PRE_BATTLE——"天生"面板
    //   事实在游戏开始前落笔，不随登场触发；用户 2026-09-20 口径。）
    ctx->grant_immunity(owner, ImmunityType::PINK_DAMAGE, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/0, kHandleYuPink,
                        /*soul_immunity=*/true, EffectScope::TEAM, /*counts=*/0,
                        /*source_effect_id=*/kDijunMarkId, ImmunityTier::Ancient);
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/0, kHandleYuAnomaly,
                        /*soul_immunity=*/true, EffectScope::TEAM, /*counts=*/0,
                        /*source_effect_id=*/kDijunMarkId, ImmunityTier::Ancient);

    if (!rt.watchers_registered) {
        rt.watchers_registered = true;

        // ── 换宠登场：罚/赐计数 + 援戳（任何己方满体力登场宠）──
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner) {
                    return;
                }
                const int enter_slot = wctx->on_stage[owner];
                ElfPet& entrant = wctx->seerRobot[owner].elfPets[enter_slot];
                // 援戳：任何己方宠登场（满体力与否都记 round，MOVE_RIGHT 侧再判 full）
                dijun_stamp_entry(entrant.on_stage_storage, wctx->roundCount,
                                  entrant.hp > 0 && entrant.hp == resolve_pet_max_hp(entrant));
                // 罚/赐：只认帝君自己登场（未消逝）
                if (enter_slot != host_slot || wctx->is_vanished(owner, host_slot)) {
                    return;
                }
                ElfPet& host = wctx->seerRobot[owner].elfPets[host_slot];
                TianqiDijunRuntime& rt = dijun_rt(host.soulmark_storage);
                ++rt.entry_count;
                dijun_punish_bless(wctx, owner, host_slot);
            },
            WatcherScope::TEAM);

        // ── 阵动态票 + 殒：死亡漏斗信号 ──
        ctx->event_center_.register_watcher(
            EventType::EVENT_DEATH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx) {
                    return;
                }
                // 殒：帝君自己被击败 → 30% 随机带走对手一只不在场存活精灵
                if (ev.target == owner && ev.slot == host_slot
                    && roll_soul_percent(*wctx, kDijunMarkId, 0, owner, 1 - owner, 30)) {
                    const int opp = 1 - owner;
                    std::vector<ChanceOption> candidates;
                    for (int slot = 0; slot < 6; ++slot) {
                        if (slot == wctx->on_stage[opp]) {
                            continue;   // 不在场限定
                        }
                        const ElfPet& pet = wctx->seerRobot[opp].elfPets[slot];
                        if (pet.hp <= 0 || wctx->is_vanished(opp, slot)) {
                            continue;
                        }
                        if (wctx->off_field_stats_protected(opp, slot)) {
                            continue;
                        }
                        candidates.push_back({slot, 1});
                    }
                    if (!candidates.empty()) {
                        const int slot = choose_soul_slot(*wctx, kDijunMarkId, 1, owner, opp, candidates);
                        wctx->seerRobot[opp].elfPets[slot].hp = 0;
                        // 走死亡漏斗：场下真2命复活（圣光灵神族）照常响应
                        g_core->defeat_pet(wctx, opp, slot, owner, DefeatCause::EFFECT);
                    }
                }
                // 阵（己方最后存活 → 免断）：每次死亡后重判（幂等授/撤）
                if (alive_count(wctx, owner) == 1
                    && wctx->seerRobot[owner].elfPets[host_slot].hp > 0) {
                    wctx->grant_immunity(owner, ImmunityType::BREAK, coverage_all(),
                                         0, 0, kHandleZhenBreak, /*soul=*/true,
                                         EffectScope::TEAM, 0, kDijunMarkId);
                } else {
                    wctx->revoke_immunity(owner, kHandleZhenBreak);
                }
                // 阵（对手最后存活 → 封回血）
                if (alive_count(wctx, 1 - owner) == 1) {
                    wctx->grant_immunity(1 - owner, ImmunityType::HEAL_BLOCK, coverage_all(),
                                         0, 0, kHandleZhenHealBlock, /*soul=*/false,
                                         EffectScope::TEAM, 0, kDijunMarkId);
                } else {
                    wctx->revoke_immunity(1 - owner, kHandleZhenHealBlock);
                }
            },
            WatcherScope::TEAM);

        // ── 御撤票：帝君消逝 → 天生免疫随之消失（武心婵 EVENT_VANISH 推送撤销同款）──
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner, host_slot](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.target != owner || ev.slot != host_slot) {
                    return;
                }
                wctx->revoke_immunity(owner, kHandleYuPink);
                wctx->revoke_immunity(owner, kHandleYuAnomaly);
                wctx->revoke_immunity(owner, kHandleZhenBreak);
                wctx->revoke_immunity(1 - owner, kHandleZhenHealBlock);
            },
            WatcherScope::TEAM);
    }

    // 部署兜底：战斗开始这只在场 = 它的"首次登场"（罚/赐）；同时给部署宠打援戳。
    // 节点随重激活/更新器重跑 → 两把 flag 幂等。
    if (!rt.battle_start_done && ctx->on_stage[owner] == host_slot) {
        rt.battle_start_done = true;
        rt.entry_count = 1;
        dijun_punish_bless(ctx, owner, host_slot);
    }
    if (!rt.deploy_stamped && ctx->roundCount == 0) {
        rt.deploy_stamped = true;
        ElfPet& deploy = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
        dijun_stamp_entry(deploy.on_stage_storage, 0,
                          deploy.hp > 0 && deploy.hp == resolve_pet_max_hp(deploy));
    }
    return EffectResult::kOk;
}

// ── 节点②：BATTLE_FIRST_MOVE_RIGHT（ROSTER）—— 帝君之援消费 ──────────────
// 桶在 preemptive memset 之后跑（battleFsm handle_BattleFirstMoveRight）→ +1 不被冲掉；
// 束缚的"先制失效"在两桶之后统一清零（官方口径）。
EffectResult soulmark_1128_aid_move_right(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    // 帝君在出战背包（存活——ROSTER 节点随阵亡停刷新，这里是防御性复核）
    if (ctx->find_pet_with_soulmark(owner, kDijunMarkId) < 0) {
        return EffectResult::kOk;
    }
    const ElfPet& onstage = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]];
    const DijunAidStamp* stamp = dijun_stamp_find(onstage.on_stage_storage);
    if (!stamp || stamp->round != ctx->roundCount || !stamp->full_hp) {
        return EffectResult::kOk;   // 本回合没登场 / 登场时非满体力
    }
    ctx->ws.preemptive_level[owner] += 1;   // "当回合所有技能先制+1"
    return EffectResult::kOk;
}

// ── 节点③：BATTLE_ROUND_START（STAGE）—— 帝君之阵 ±10% 每回合重写 ─────────
// ws 加算通道每回合 reset → 节点每回合重写（776/32 套路）。阵亡数实时读 count_dead(ALL)。
EffectResult soulmark_1128_array_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !g_core || !g_core->count_dead) {
        return EffectResult::kOk;
    }
    const int dead = g_core->count_dead(ctx, owner, DeadScope::ALL);
    if (dead <= 0) {
        return EffectResult::kOk;
    }
    // 受伤 -10%×dead：加算减伤槽（REDUCE_PCT；抢第一个空槽，与其他减伤来源并存）
    for (int i = 0; i < 4; ++i) {
        if (ctx->ws.damage_reduce_add[owner][i] == 0) {
            ctx->ws.damage_reduce_add[owner][i] = 10 * dead;
            break;
        }
    }
    // 造成伤害 +10%×dead：通用增伤·加算（AMP 阶段求和）
    ctx->ws.damage_add_pct[owner] += static_cast<float>(10 * dead);
    return EffectResult::kOk;
}

// ── 节点④：FIRST/SECOND_AFTER_ACTION（STAGE）—— 帝君之阵"使用技能后" ────
// 对手每有一只不在场存活精灵 → 恢复 1/10 最大体力 + 等量百分比伤害（903 之赐同款双桶）。
EffectResult soulmark_1128_array_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !used_skill_this_round_local(ctx, owner)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(owner);
    if (host.soulMark.id != kDijunMarkId || host.hp <= 0) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    int off_field_alive = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[opp]) {
            continue;
        }
        const ElfPet& pet = ctx->seerRobot[opp].elfPets[slot];
        if (pet.hp > 0 && !ctx->is_vanished(opp, slot)) {
            ++off_field_alive;
        }
    }
    if (off_field_alive <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = resolve_pet_max_hp(host);
    const int amount = max_hp / 10 * off_field_alive;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    // 等量绑定**实际回到的血**（532 口径：满血/被封回血 → 回 0 → 无伤害）
    const int before = host.hp;
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    const int healed = ctx->getPet(owner).hp - before;
    if (healed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// ── 帝君之御的抗性（2026-11-22 SP④拆分线迁移）────────────────────────────
// "天生……将固定伤害和百分比伤害始终抗性视为100%"原走战前补丁写 pet.damage_resist
// 本体——但语料 post497 把帝君明确列进 SP④"会被无视的抗性提升"清单：写本体 = 混进
// base 基线，④穿透（读 base）永远穿不动它。已迁 ws **boost 层**（per-slot 增量、
// 每回合 ROUND_START 重写，见 dijun_1128_resist_boost_round_start）：
//   eff = clamp(base + boost) —— 无 SP④ 时行为不变（boost 100 → clamp 封顶 100）；
//   SP④武装时消费读 base → 帝君抗性回落本体（官方口径）。

}  // namespace

// 御（boost 层版）：每回合 ROUND_START 给自己槽位写 fixed/percent=100 增量。
// ROSTER scope——帝君存活于背包即提供（"天生…始终"）；阵亡停刷新 → 下回合 boost=0
// → eff 回本体。add_resist_boost 内部在场即重算 eff，时序：ROUND_START resetWorkspace
// → sync（boost=0 → eff=base）→ 本桶写入（eff=100）→ 首段攻击结算读到 100，与旧
// 本体模型逐点等价（2026-11-22 迁移记档）。
EffectResult dijun_1128_resist_boost_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int holder = ctx->find_mark_in_roster(owner, kDijunMarkId);
    if (holder < 0) {
        return EffectResult::kOk;   // 存活于出战背包前提（阵亡/消逝不提供）
    }
    // 御 = 魂印赋予（"天生"）→ **中层 SoulMark**（三层模型 2026-11-22：常态参考层；
    // 星皇二段读 base+mid 时帝君抗性照常生效，SP④才穿到 base）。
    ctx->add_resist_boost(owner, holder, BattleContext::ResistLayer::SoulMark,
                          /*crit=*/0, /*fixed=*/100, /*percent=*/100);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册）──
SOUL_PROGRAM(kDijunMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1128_arm,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_1128_aid_move_right,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &dijun_1128_resist_boost_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::ROSTER),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_1128_array_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION,
                              &soulmark_1128_array_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION,
                              &soulmark_1128_array_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
