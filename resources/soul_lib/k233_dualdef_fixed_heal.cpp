// K233 · 双防和固定伤+回血族 —— effect 844（1 宏 2 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k233）
//
// 行清单（tools/soul_effects_dump.py 844，2026-10-08）：
//   844 row552（月之子·奇拉美 3227）args[50,50,1]
//     「攻击附加对手双防之和50%的固定伤害；攻击恢复自身双防之和50%的体力（BOSS无效）」
//   844 row553（同载体 3227，伙伴特性）args[60,60,1]
//     同文 60%（行间互锚唯一变量）。
//   → db[0]=攻击附加固伤比%（对手双防和）；db[1]=攻击回血比%（自身双防和）；
//     db[2]=1 不消费（K2 纪律 + 工单行标注记档）。tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   - 锚点 = K1 766 同款 EVENT_TAKE_DAMAGE watcher（arm-once）：ev.actor==own +
//     ev.target==opp + ev.state ∈ {FIRST,SECOND_ATTACK_DAMAGE}（只认攻击伤害时点，
//     固定/真伤/粉伤不构成"攻击"）+ ev.amount>0 + onstage 宿主 →
//     ① deal_pink_damage(对手双防和×db[0]%, FIXED)（"固定伤害"=FIXED 档粉伤，
//     766/K197 同原语，吃护罩）；② heal_amount(自身双防和×db[1]%)（原语：吃封回血
//     +恢复修正+满血封顶）；
//   - "双防之和" = numericalBase[DEFENSE]+[SPECIAL_DEFENSE]（面板权威基线，K8 口径；
//     等级增减改视图不改面板）；
//   - 属性技（无伤害管线）天然不触发；miss 无 TAKE_DAMAGE 事件同免；
//   - （BOSS无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08）：①内核 grep 844 无魂印行为硬编码（moves_lib/
//   j_crit_miss_drain 的 844=吸取系另一空间切片——eid 撞号甄别 SOP §六.2）；
//   ②effect_icon kind=[5,14]、tips 为权威文（dump 原文已核）；③soul_lib 无 844
//   注册、sibling 分支 feat/k226-k232 亦无（全分支 grep）。
// 先例：K1 766（攻击附加固伤 TAKE_DAMAGE 形 + FIXED 档）、K10 693（攻附+回血双段
//   语义）、K8 面板（numericalBase 权威基线）、K226（arm-once wired 旗）。
// 场景回归：test/scenario/scenario_3770_dualdef_fixed_heal_844.cpp（号段 3770-3779）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 844;
constexpr int kWiredKeyBase = 991432000;   // +owner：watcher 已挂旗标（防 early 双触发）

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

int def_sum(const ElfPet& pet) {
    return pet.numericalBase[NumericalPropertyIndex::DEFENSE]
         + pet.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
}

// ── early：挂一次"攻击双附加"watcher（K1 766 同款门）────────────────────────

EffectResult soulmark_844_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int attach_pct = db_at(row, 0, -1);
    const int heal_pct = db_at(row, 1, -1);
    if (attach_pct <= 0 && heal_pct <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, attach_pct, heal_pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.amount <= 0) {
                return;   // 只认"自己攻击对手"造成的伤害
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            // 只认攻击伤害时点（K1 766 口径：固定/真伤/粉伤不构成"攻击"）。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            const int opp = 1 - owner;
            const ElfPet& foe = wctx->getPet(opp);
            const ElfPet& self = wctx->getPet(owner);
            if (foe.hp <= 0 || self.hp <= 0) {
                return;
            }
            if (attach_pct > 0 && g_core && g_core->deal_pink_damage) {
                const int amount = def_sum(foe) * attach_pct / 100;
                if (amount > 0) {
                    (void)g_core->deal_pink_damage(wctx, opp, amount,
                                                   DamageKind::FIXED, owner);
                }
            }
            if (heal_pct > 0 && g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(wctx, owner, def_sum(self) * heal_pct / 100);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：844）。early=双附加 watcher arm-once（更新器重注册不影响事件面，SOP §一.1）。
SOUL_PROGRAM(844, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_844_arm,
                    /*once=*/false, /*early=*/true)});
