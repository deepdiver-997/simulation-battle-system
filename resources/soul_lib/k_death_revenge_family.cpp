// K11 免疫/重生/死亡反击族 —— effect 221/304/324/392（4 宏 7 行）
//
// 行清单（tools/soul_effects_dump.py 221 304 324 392，2026-10-04）：
//   221 2 行 args空（2476 / [1864,1865]）：「出战时，首回合免疫所有异常状态」
//       → ENTER（early）grant_immunity(ANOMALY, mask=0 全异常, duration 1)——
//       每次登场重挂；沿用 RuleCenter 的绝对窗口，不 tick，死切起算规则不变。
//   304 2 行 args[pct,次数]（同 id 双行 args 驱动）：[50,1]/[100,1]「受到致死攻击时
//       X%概率重生一次，重生时清除敌我双方能力上升和下降状态」
//       → TAKE_DAMAGE watcher 致死门（hp==0）+ roll + revive_pet（回满，HP 值记档：
//       官方文本未给重生血量，按"重生=满状态"通例回满）+ clear_stat_boosts 双方 +
//       stat_drop？——「清除上升和下降」= clear_stat_boosts + clear_stat_drops 双方
//       （undying_1067 同族邻接；1067 只清提升，本行含下降 → clear_stat_drops 补齐）
//   324 1 行 args[1,0]（2648 咤克斯？——行归 2648）：「若被对手击败则将对手的体力降为
//       1点，并清除双方所有能力提升状态」
//       → FIRST/SECOND_MOVER_DEATH 桶节点：宿主已倒 && last_damage_actor==对手 →
//       对手 hp 直写 1（"降为1"非秒杀，不走 force_hp_to_zero 的免秒杀查询）+
//       clear_stat_boosts 双方（common.h find_defeated_foe_slot 归因门同款判据）
//   392 2 行 args[2,0]（2818/2870 雷锘同文）：「战斗阶段结束时若体力低于1/2，并且自身
//       体力低于对手，则将对手的体力变成和自己的体力一致」
//       → BATTLE_ROUND_END 桶：双门（self.hp*2 < max 且 self.hp < opp.hp）→ opp.hp 直写
//
// 三查记录（2026-10-04）：①内核无四 id 硬编码；②effect_info 同 id 为技能空间模板；
// ③registry 无四 id，未合并分支无实现。
#include "common.h"

namespace {

// ── 宿主行参数视图（各文件自治同款）────────────────────────────────────────

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

bool armed_once(BattleContext* ctx, int owner, int key) {
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── 221：出战首回合全异常免疫 ──────────────────────────────────────────────

EffectResult soulmark_221_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 221)) {
        return EffectResult::kOk;
    }
    // 句柄交给 RuleCenter 分配；覆盖键 (owner, effect=221, type) 使 early+桶幂等。
    // 固定正值句柄会先命中全局句柄刷新路径，让双方 221 串票。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/1, /*source_id=*/0,
                        /*soul_immunity=*/true, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/221);
    return EffectResult::kOk;
}

// ── 304：受致死攻击概率重生（清双方升降）───────────────────────────────────

EffectResult soulmark_304_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_once(ctx, owner, 304 * 100 + 7)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 304);
    const int pct = db_at(row, 0, 50);
    const int charges = db_at(row, 1, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct, charges](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || !g_core) {
                return;
            }
            ElfPet& self = wctx->getPet(owner);
            auto& storage = self.soulmark_storage;
            auto it = storage.find(304 * 100 + 1);
            if (it == storage.end()) {
                it = storage.emplace(304 * 100 + 1, int{0}).first;
            }
            int* used = std::any_cast<int>(&it->second);
            if (!used || *used >= charges) {
                return;
            }
            if (self.hp > 0) {
                return;   // 致死门（1067 同款：hp==0 即门，不挑事件类型）
            }
            if (!roll_percent(pct)) {
                return;
            }
            *used += 1;
            const int slot = wctx->on_stage[owner];
            if (g_core->revive_pet) {
                g_core->revive_pet(wctx, owner, slot, resolve_pet_max_hp(self));
            }
            if (g_core->clear_stat_boosts) {
                g_core->clear_stat_boosts(wctx, owner);
                g_core->clear_stat_boosts(wctx, wctx->opponent(owner));
            }
            if (g_core->clear_stat_drops) {
                g_core->clear_stat_drops(wctx, owner);
                g_core->clear_stat_drops(wctx, wctx->opponent(owner));
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 324：被击败→对手降 1 体 + 清双方强化 ──────────────────────────────────

EffectResult soulmark_324_death(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 324)) {
        return EffectResult::kOk;
    }
    // 宿主已倒（死亡时点桶）+ 最后一击来自对手 → 复仇。
    const int slot = ctx->on_stage[owner];
    if (ctx->seerRobot[owner].elfPets[slot].hp > 0) {
        return EffectResult::kOk;
    }
    const int foe = 1 - owner;
    if (ctx->last_damage_actor[owner][slot] != foe) {
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->seerRobot[foe].elfPets[ctx->on_stage[foe]];
    if (opp.hp > 0) {
        opp.hp = 1;   // "降为1点"：非秒杀语义，不走 force_hp_to_zero 免秒杀查询
    }
    if (g_core && g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, owner);
        g_core->clear_stat_boosts(ctx, foe);
    }
    return EffectResult::kOk;
}

// ── 392：回合末低体门体力对齐 ─────────────────────────────────────────────

EffectResult soulmark_392_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 392)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    ElfPet& opp = ctx->getPet(ctx->opponent(owner));
    if (self.hp <= 0 || opp.hp <= 0) {
        return EffectResult::kOk;
    }
    const int self_max = resolve_pet_max_hp(self);
    if (self_max > 0 && self.hp * 2 < self_max && self.hp < opp.hp) {
        opp.hp = self.hp;   // "对手的体力变成和自己的体力一致"
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +4（只增不改）────────────────
SOUL_PROGRAM(221, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_221_enter,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(304, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_304_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(324, {SoulMarkNodeRef(State::BATTLE_FIRST_MOVER_DEATH, &soulmark_324_death),
                   SoulMarkNodeRef(State::BATTLE_SECOND_MOVER_DEATH, &soulmark_324_death)});
SOUL_PROGRAM(392, {SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_392_round_end)});
