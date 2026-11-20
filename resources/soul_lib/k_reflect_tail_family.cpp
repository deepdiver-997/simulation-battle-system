// K15 反弹散点收尾 —— effect 106/110/133（3 宏 3 行）
//
// 行清单（effect_icon 直查 + tools/soul_effects_dump.py，2026-10-05）：
//   106 2672 args[60,0]：「每次受到伤害则反弹对手60点固定伤害」
//       → TAKE_DAMAGE watcher（K3 flat_reflect 同款；ev.actor 范围守卫必带——
//       无行动方事件 actor=-1）→ 对 actor 反弹 db[0] 定额 FIXED
//   110 2681 args空（神·阿波罗；183 姊妹行已改文）：「反弹对手给予的能力下降」
//       → EVENT_STAT_CHANGED watcher：ev.target==owner 且 level_delta<0 →
//       对对手同槽同幅 stat_drop（查免弱）。⚠️ 事件无 actor 恒 -1（783 先例同款
//       归因近似）——"对手给予的"按对场对手落地，自清/消除不触发（dispel 不走
//       write_ability_level 事件面）
//   133 2736 args[50,19]：「免疫并反弹对手造成的致命一击，死亡时对手有50%概率瘫痪」
//       → BLOCK 票 isCrit 门（K1 150 去概率版）：免疫本次（归零）+ 按原伤定额
//       FIXED 反弹攻方；MOVER_DEATH 桶 + last_damage_actor 归因门 → roll db[0]%
//       → 对击杀者 apply_anomaly 瘫痪 db[1]（2 回合）
//
// 三查记录（2026-10-05，立单时已做）：内核零硬编码、effect_icon kind 已核、registry
// 无三 id（pet 绑定 2672/2681/2736）。
#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kParalysisId = static_cast<int>(AbnormalStatusId::Crippled);      // 瘫痪=19（官方码；Paralysis=0 麻痹两码事）

bool armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(storage_key);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(storage_key, int{-1}).first;
    }
    int* armed_epoch = std::any_cast<int>(&it->second);
    if (!armed_epoch) {
        return true;
    }
    if (*armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    *armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

bool armed_once(BattleContext* ctx, int owner, int key) {
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

// ── 106：每次受到伤害反弹 60 点 ────────────────────────────────────────────

EffectResult soulmark_106_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 106 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 106);
    const int amount = db_at(row, 0, 60);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, amount](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor < 0 || ev.actor > 1
                || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, 106)) {
                return;
            }
            if (g_core && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(wctx, ev.actor, amount, DamageKind::FIXED, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 110：能力下降反弹 ─────────────────────────────────────────────────────

EffectResult soulmark_110_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_once(ctx, owner, 110 * 100 + 7)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_CHANGED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.level_delta >= 0) {
                return;   // 只认自身被降
            }
            if (!onstage_has_soul_mark(wctx, owner, 110)) {
                return;
            }
            if (g_core && g_core->stat_drop) {
                g_core->stat_drop(wctx, wctx->opponent(owner), ev.stat_index,
                                  -ev.level_delta, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 133：免疫并反弹致命一击 + 死亡瘫痪 ────────────────────────────────────

EffectResult soulmark_133_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 133 * 100 + 9)) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::BLOCK, owner, DamageEffectCategory::BLOCK,
        [owner](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || !d.isCrit || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 133)) {
                return;
            }
            const int reflected = d.final;   // 免疫前快照（K1 150 同位）
            d.base = 0;
            d.afterAdd = 0;
            d.afterMul = 0;
            d.final = 0;
            d.addPct = 0;
            d.mulCoef = 0.0;
            if (d.attackerId >= 0 && g_core && g_core->deal_pink_damage) {
                g_core->deal_pink_damage(c, d.attackerId, reflected, DamageKind::FIXED,
                                         bucket_owner);
            }
        });
    return EffectResult::kOk;
}

EffectResult soulmark_133_death(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 133)) {
        return EffectResult::kOk;
    }
    const int slot = ctx->on_stage[owner];
    if (ctx->seerRobot[owner].elfPets[slot].hp > 0) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;
    if (ctx->last_damage_actor[owner][slot] != foe) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 133);
    const int pct = db_at(row, 0, 50);
    const int anomaly = db_at(row, 1, kParalysisId);
    if (!roll_percent(pct)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->apply_anomaly) {
        g_core->apply_anomaly(ctx, foe, anomaly, /*rounds=*/2, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（只增不改）────────────────
SOUL_PROGRAM(106, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_106_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(110, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_110_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(133, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_133_arm,
                   /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVER_DEATH, &soulmark_133_death),
                   SoulMarkNodeRef(State::BATTLE_SECOND_MOVER_DEATH, &soulmark_133_death)});
