// 莫塔里安 4541 —— 登场护罩 + 击败重触发族（完整版）
//
// 官方魂印（4399 图鉴问答页 pet_id 4541）：
//   「登场时获得300点护罩，回合开始时若此护罩存在则当回合自身免疫异常状态且对手攻击后
//     速度-2，若对手不处于回合类效果则额外进入束缚，若对手处于回合类效果则自身全属性+1；
//     当回合未击败对手时若对手处于能力下降则令对手害怕，
//     击败对手时重新触发自身登场时效果并抵挡下次受到的攻击（boss无效）」
//
// 落地（2026-09-20）：
//   ① 登场 300 点护罩（early+STAGE；重触发按"重新获得"刷新回 300）。
//   ② 击败对手时（OPPONENT_DEFEAT_STATUS，STAGE、槽级归因）：重触发主体登场效果
//      （CoreApi::retrigger_entrance）+ 抵挡下次受到的攻击（DAMAGE 票 counts=1）。
//   ③ 回合开始 riders（ROUND_START，STAGE）：登场护罩（source_id=4541）仍在 →
//      a. 当回合免疫异常（ANOMALY 窗口票，回合内有效）；
//      b. 对手每次攻击后速度-2（stat_drop，查免弱）；
//      c. 对手不处于回合类效果 → 束缚（正常施加通道）；处于 → 自身全属性+1。
//      "回合类效果"引擎口径 = 对手三个效果桶的回合类计数之和 > 0
//      （TimedBucket::active_round_count；套装条目恒 TEAM 永久不计入）。
//   ④ 未击败分支（OPPONENT_DEFEAT_STATUS，无击败归因）：对手处于任一能力下降 → 害怕。
#include "common.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 SoulMark.id 按 effect_id 查程序表）。
//    4541 是 monster id，只用于命名与注释——错键会让程序在真实对局挂不上
//    （2026-09-27 修复；effect_id 取自 unity 库 effect_icon 1751 行 [4541]→2142）。
constexpr int kMortarionMarkId = 2142;
constexpr int kEntranceHood = 300;

// ① 登场：300 点护罩（魂印专属优先级；同 source_id 重触发刷新回 300，不叠条目）。
EffectResult soulmark_4541_entrance_hood(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0) {
        return EffectResult::kOk;
    }
    // 官方口径（用户 2026-09-20）：登场护罩**下场不保留**、每次重新登场都刷新。
    // on_exit 先清 0（复用同一条目），这里 refresh 回 300——首次登场才需要 add。
    ShieldBank& hood = ctx->seerRobot[owner].elfPets[self_slot].hood_bank_;
    hood.refresh_source(kMortarionMarkId, kEntranceHood);
    if (hood.quantity_of(kMortarionMarkId) == 0) {
        hood.add(ShieldBank::kSoulShieldPriority, kEntranceHood, kMortarionMarkId,
                 /*duration=*/0);   // 0 = 破罩前持续
    }
    return EffectResult::kOk;
}

// 登场护罩下场不保留：离场钩子把该 source 条目清 0（riders 的依赖随之失效）。
// 他的节点全为 STAGE（无 ROSTER 节点）→ perform_switch/阵亡补位都会走 deactivate → 本钩子。
void mortarion_exit_clear_hood(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    for (int slot = 0; slot < 6; ++slot) {
        ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
        if (pet.soulMark.id == kMortarionMarkId) {
            pet.hood_bank_.refresh_source(kMortarionMarkId, 0);
        }
    }
}

// ② 击败对手时：重新触发主体登场效果 + 抵挡下次受到的攻击。
EffectResult soulmark_4541_defeat_retrigger(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int subject_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || subject_slot < 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_vanished(owner, subject_slot)) {
        return EffectResult::kOk;   // 主体已消逝
    }
    if (find_defeated_foe_slot(ctx, owner) < 0) {
        return EffectResult::kOk;   // 本回合没有"主体方击败对手"
    }
    // 重新触发登场效果——主体是谁就触发谁的（阿尔忒弥斯副本 → 她自己的登场效果）。
    if (g_core && g_core->retrigger_entrance) {
        g_core->retrigger_entrance(ctx, owner, subject_slot);
    }
    // 抵挡下次受到的攻击：DAMAGE 票 counts=1（管线 BLOCK 阶段消费一次后消失）。
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/0,
                        /*source_id=*/kMortarionMarkId, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/1,
                        /*source_effect_id=*/kMortarionMarkId);
    return EffectResult::kOk;
}

// 对手是否处于回合类效果：三个效果桶的回合类计数（duration>0 的条目）之和。
// 套装桶条目恒 TEAM/duration=-1（isRoundEffect()=false）→ 天然不计入，不用查。
bool foe_has_round_effects(BattleContext* ctx, int owner) {
    const int foe = 1 - owner;
    return ctx->soul_mark_effects.active_round_count(foe) > 0
        || ctx->skills_effects.active_round_count(foe) > 0;
}

// ③ 回合开始 riders：登场护罩仍在才结算（"此护罩" = 登场获得的那笔，source_id=4541）。
EffectResult soulmark_4541_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int self_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || self_slot < 0 || ctx->on_stage[owner] != self_slot) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->seerRobot[owner].elfPets[self_slot];
    if (self.hood_bank_.quantity_of(kMortarionMarkId) <= 0) {
        return EffectResult::kOk;   // 护罩不在：本回合 riders 不结算
    }

    // a. 当回合自身免疫异常（窗口票：InRounds 1 回合 = 本回合；source 覆盖键刷新）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/kMortarionMarkId, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/kMortarionMarkId);

    // c. 对手不处于回合类效果 → 束缚；处于 → 自身全属性+1。
    if (g_core && g_core->apply_anomaly) {
        if (!foe_has_round_effects(ctx, owner)) {
            g_core->apply_anomaly(ctx, 1 - owner, static_cast<int>(AbnormalStatusId::Bind),
                                  /*duration_rounds=*/2, /*actor=*/owner);
        } else {
            for (int stat = 0; stat <= 4; ++stat) {   // 攻/防/特攻/特防/速 全属性+1
                if (g_core->stat_change) {
                    g_core->stat_change(ctx, owner, stat, /*delta=*/+1);
                }
            }
        }
    }

    // b. 对手每次攻击后速度-2（本回合内，watcher 一场挂一次、回合窗口内守卫）。
    const int watch_key = 990454101 + owner;
    if (!ctx->plugin_storage.count(watch_key)) {
        ctx->plugin_storage[watch_key] = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_HIT, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor < 0 || ev.actor > 1 || !g_core || !g_core->stat_drop) {
                    return;
                }
                const int owner = 1 - ev.actor;   // 莫塔里安方 = 攻击者的对手
                // 护罩不在 → riders 整体不结算（含速度-2）。
                const int slot = wctx->on_stage[owner];
                if (wctx->seerRobot[owner].elfPets[slot].hood_bank_.quantity_of(kMortarionMarkId)
                    <= 0) {
                    return;
                }
                // 只认攻击技能（视图；同 2212 口径），属性技不触发。
                if (wctx->ws.skill_type_view[ev.actor]
                    == static_cast<int>(SkillType::Attribute)) {
                    return;
                }
                g_core->stat_drop(wctx, ev.actor, /*stat=*/4 /*速度*/, /*amount=*/2,
                                  /*actor=*/owner);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ④ 未击败分支：对手处于任一能力下降 → 害怕（正常施加通道）。
EffectResult soulmark_4541_not_defeat(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    const int subject_slot = resolve_subject_slot_from_args(args);
    if (owner < 0 || owner > 1 || subject_slot < 0) {
        return EffectResult::kOk;
    }
    if (ctx->is_vanished(owner, subject_slot)) {
        return EffectResult::kOk;
    }
    if (find_defeated_foe_slot(ctx, owner) >= 0) {
        return EffectResult::kOk;   // 本回合击败了 → 走 ② 重触发，不是未击败分支
    }
    const int foe = 1 - owner;
    // 官方口径（机制解析 idx=389 #4 同族语义）：未击败分支只在对手未死亡时结算。
    if (ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]].hp <= 0) {
        return EffectResult::kOk;
    }
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(foe, stat) < 0) {
            if (g_core && g_core->apply_anomaly) {
                g_core->apply_anomaly(ctx, foe, static_cast<int>(AbnormalStatusId::Fear),
                                      /*duration_rounds=*/2, /*actor=*/owner);
            }
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kMortarionMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4541_entrance_hood,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4541_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_OPPONENT_DEFEAT_STATUS, &soulmark_4541_defeat_retrigger,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_OPPONENT_DEFEAT_STATUS, &soulmark_4541_not_defeat,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
SOUL_MARK_HOOKS(kMortarionMarkId, nullptr, &mortarion_exit_clear_hood);
