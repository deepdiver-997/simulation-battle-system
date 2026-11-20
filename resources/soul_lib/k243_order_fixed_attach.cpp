// K243 · 先后手双段固定伤族 —— effect 1019（1 宏 1 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k243）
//
// 行清单（tools/soul_effects_dump.py 1019，2026-10-13）：
//   1019 row735（埃兰 3417）args[4,200,3,50,250,4,1]（kind[5,14]，单行）
//     「先出手时附加对手最大体力1/4的固定伤害，若附加固定伤害高于200则恢复自身最大
//     体力的1/3；后出手时附加自身攻击值50%的固定伤害，若附加固定伤害高于250则{吸取}
//     对手最大{体力}的1/4（BOSS无效）」
//   → db[0]=先手附加分母（对手 max/db[0]）；db[1]=先手回复门（附加 > db[1]）；
//     db[2]=先手回复分母（max/db[2]）；db[3]=后手附加比%（自身攻击值）；
//     db[4]=后手吸取门（附加 > db[4]）；db[5]=后手吸取分母（对手 max/db[5]）；
//     db[6]=1 不消费（K2 纪律记档）。tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   - 锚点 = EVENT_TAKE_DAMAGE watcher arm-once（K1 766 门：ev.actor==own +
//     ATTACK_DAMAGE 时点 + amount>0 + onstage）→ **is_second_mover 消费时点直读**
//     分流（K235 记档：先手权伤害时点已结算）：
//     - 先手：deal_pink_damage(对手 max/db[0], FIXED)（K233/K1 FIXED 档）→
//       附加 > db[1] → heal_amount(max/db[2])；
//     - 后手：deal_pink_damage(自身攻击值×db[3]%, FIXED)（"攻击值"=数值面板
//       PHYSICAL_ATTACK 权威基线，K8/K233 口径）→ 附加 > db[4] →
//       {吸取} 对手 max/db[5]（deal PERCENT_VALUE + heal 等量，K225 双段值形）；
//   - 门为严格大于（"高于"）；回复吃封回血/修正/封顶（原语自理）；
//   - （BOSS无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-13，author k-prod-6 第三轮）：①内核 grep 1019 无魂印行为
//   硬编码；②effect_icon kind=[5,14]、tips 为权威文（dump 原文已核）；③registry 无
//   1019、soul_lib 并集（hub+feat/k226-k242）无 1019——真缺口。
// 先例：K235 705（is_second_mover 消费时点直读）、K1 766（攻击窗 watcher）、
//   K233 844（FIXED 附加+面板读数）、K225 341（吸取双段值形）、K226（arm-once 旗）。
// 场景回归：test/scenario/scenario_3870_order_fixed_attach_1019.cpp（号段 3870-3879）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1019;
constexpr int kWiredKeyBase = 991444000;   // +owner：watcher 已挂旗标

int atk_panel(const ElfPet& pet) {
    return pet.numericalBase[NumericalPropertyIndex::PHYSICAL_ATTACK];
}

// ── 宿主行参数（"行≠宏"，K226 同款私副本）────────────────────────────────────

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

// ── early：先后手双段附加 watcher ───────────────────────────────────────────

EffectResult soulmark_1019_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int first_denom = db_at(row, 0, -1);
    const int first_gate = db_at(row, 1, -1);
    const int first_heal_denom = db_at(row, 2, -1);
    const int second_pct = db_at(row, 3, -1);
    const int second_gate = db_at(row, 4, -1);
    const int second_drain_denom = db_at(row, 5, -1);
    if (first_denom <= 0 || first_gate < 0 || first_heal_denom <= 0 || second_pct <= 0
        || second_gate < 0 || second_drain_denom <= 0) {
        return EffectResult::kOk;   // 参数不齐不武装（记档）
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, first_denom, first_gate, first_heal_denom, second_pct, second_gate,
         second_drain_denom](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.amount <= 0) {
                return;
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const int opp = 1 - owner;
            const ElfPet& foe = wctx->getPet(opp);
            const ElfPet& self = wctx->getPet(owner);
            if (foe.hp <= 0 || self.hp <= 0) {
                return;
            }
            if (!wctx->is_second_mover(owner)) {
                // 先出手：附加 对手 max/first_denom；高于 first_gate → 回 max/first_heal
                const int attach = resolve_pet_max_hp(foe) / first_denom;
                if (attach > 0 && g_core && g_core->deal_pink_damage) {
                    (void)g_core->deal_pink_damage(wctx, opp, attach,
                                                   DamageKind::FIXED, owner);
                }
                if (attach > first_gate && g_core && g_core->heal_amount) {
                    (void)g_core->heal_amount(wctx, owner,
                                              resolve_pet_max_hp(self) / first_heal_denom);
                }
                return;
            }
            // 后出手：附加 自身攻击值×second_pct%；高于 second_gate → 吸取对手 max/denom
            const int attach = atk_panel(self) * second_pct / 100;
            if (attach > 0 && g_core && g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(wctx, opp, attach,
                                               DamageKind::FIXED, owner);
            }
            if (attach > second_gate) {
                const int drain = resolve_pet_max_hp(foe) / second_drain_denom;
                if (drain <= 0) {
                    return;
                }
                if (g_core && g_core->deal_pink_damage) {
                    (void)g_core->deal_pink_damage(wctx, opp, drain,
                                                   DamageKind::PERCENT_VALUE, owner);
                }
                if (g_core && g_core->heal_amount) {
                    (void)g_core->heal_amount(wctx, owner, drain);
                }
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1019）。early=先后手双段附加 watcher（事件面）。
SOUL_PROGRAM(1019, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1019_arm,
                    /*once=*/false, /*early=*/true)});
