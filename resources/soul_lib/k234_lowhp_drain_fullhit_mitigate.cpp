// K234 · 低体吸取+满体受击减伤族 —— effect 836（1 宏 2 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k234）
//
// 行清单（tools/soul_effects_dump.py 836，2026-10-08）：
//   836 row538（海瑟薇 3211）args[2,3,1,50,1]
//     「自身体力低于1/2时，攻击{吸取}对手最大{体力}的1/3；自身满体力时，若受到攻击
//     则下1次受到的伤害减少50%（BOSS无效）」
//   836 row549（同载体 3211）args[2,3,1,65,1]
//     同文；db[3]=减伤 65%（行间互锚唯一变量）。
//   → db[0]=低体门分母（自身体力 < max/db[0]）；db[1]=吸取分母（对手 max/db[1]）；
//     db[2]=减伤次数（"下1次"=1）；db[3]=减伤%；db[4]=1 不消费（K2 纪律记档）。
//     tips 为权威文（SOP §六.1）。
//
// 实现口径（early 三件套，EpochGuard 防开局双挂，K184/K197 同形）：
//   ① 低体吸取 = EVENT_TAKE_DAMAGE watcher（K1 766 门：ev.actor==own +
//     ATTACK_DAMAGE 时点 + amount>0）→ 自身 < max/db[0] →
//     deal_pink_damage(对手 max/db[1], PERCENT_VALUE) + heal_amount(同值)
//     （K225 341 双段同款；"吸取"=等量回复）；
//   ② 满体装填 = DETECT 相位管线回调（K197：DETECT 时点红伤未扣——现值即受击前
//     体力）：d.defenderId==own + isRed + final>0 + 自身满体力（== max）→
//     次数账本装 db[2] 发（已装不重叠）；BLOCK 归零打击不装（final>0 门，记档）；
//   ③ 减伤消费 = REDUCE_PCT + MITIGATE 次数票（K184 同形）：有次数 →
//     d.final×(100-db[3])/100 + 次数-1（只红伤；粉伤不消费记档）。
//   时序自洽：同一次红伤 REDUCE_PCT（先，消费）→ DETECT（后，装填）——满体首击
//   全额、次击减伤、击后非满体不再装填（场景 3780.c 三连击实证）。
//   （BOSS无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08）：①内核 grep 836 无魂印行为硬编码（k_guard_family 的
//   4836 系另一 eid——撞号甄别 SOP §六.2）；②effect_icon kind=[3,5]、tips 为权威文；
//   ③soul_lib 无 836 注册、sibling 分支 feat/k226-k233 亦无（全分支 grep）。
// 先例：K225 341（吸取双段值形）、K184 272（REDUCE_PCT 次数票+EpochGuard）、
//   K197 259（DETECT 相位现值读）、K1 766（TAKE_DAMAGE 攻击门）。
// 场景回归：test/scenario/scenario_3780_lowhp_drain_fullhit_mitigate_836.cpp（号段 3780-3789）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 836;
constexpr int kWiredKeyBase = 991433000;   // +owner：early 已挂旗标（epoch 内防重）
constexpr int kChargeBase = 991434000;     // +owner：减伤剩余次数

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K226 同款私副本）──────────

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
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
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int charge_of(BattleContext* ctx, int owner) {
    const auto it = ctx->plugin_storage.find(kChargeBase + owner);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// ── early：三件套一次挂（epoch 防重）────────────────────────────────────────

EffectResult soulmark_836_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int low_denom = db_at(row, 0, -1);
    const int drain_denom = db_at(row, 1, -1);
    const int charges = db_at(row, 2, -1);
    const int reduce_pct = db_at(row, 3, -1);
    if (low_denom <= 0 || drain_denom <= 0 || charges <= 0 || reduce_pct <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 开局 early 双触发防重
    }
    ctx->plugin_storage.emplace(key, 1);

    // ③ 减伤消费票（K184 同形：REDUCE_PCT + MITIGATE）
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, reduce_pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMark)) {
                return;
            }
            if (charge_of(c, bucket_owner) <= 0) {
                return;
            }
            d.final = d.final * (100 - reduce_pct) / 100;
            auto& ch = c->plugin_storage[kChargeBase + bucket_owner];
            ch = any_int_or(ch, 0) - 1;   // 消费一次
        });

    // ② 满体装填（DETECT：红伤未扣，现值即受击前体力）
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, charges](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMark)) {
                return;
            }
            const ElfPet& self = c->getPet(bucket_owner);
            if (self.hp <= 0 || self.hp != resolve_pet_max_hp(self)) {
                return;   // 满体门（非满不装）
            }
            if (charge_of(c, bucket_owner) > 0) {
                return;   // 已装不重叠
            }
            c->plugin_storage[kChargeBase + bucket_owner] = charges;
        });

    // ① 低体吸取 watcher（K1 766 门 + K225 双段值形）
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, low_denom, drain_denom](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.amount <= 0) {
                return;   // 只认"自己攻击对手"造成的伤害
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const ElfPet& self = wctx->getPet(owner);
            const ElfPet& foe = wctx->getPet(1 - owner);
            const int max_hp = resolve_pet_max_hp(self);
            if (self.hp <= 0 || foe.hp <= 0 || max_hp <= 0) {
                return;
            }
            if (self.hp >= max_hp / low_denom) {
                return;   // 低体门：严格低于 max/db[0]
            }
            const int drain = resolve_pet_max_hp(foe) / drain_denom;
            if (drain <= 0) {
                return;
            }
            if (g_core && g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(wctx, 1 - owner, drain,
                                               DamageKind::PERCENT_VALUE, owner);
            }
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(wctx, owner, drain);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：836）。early=减伤票/满体装填/低体吸取三件套（事件面与管线票不受
// 更新器重注册影响，SOP §一.1）。
SOUL_PROGRAM(836, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_836_arm,
                    /*once=*/false, /*early=*/true)});
