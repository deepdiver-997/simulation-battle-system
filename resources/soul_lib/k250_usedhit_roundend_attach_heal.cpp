// K250 · 攻击/命中门回合末固定伤+回复族 —— effect 1536（1 宏 1 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k250）
//
// 行清单（tools/soul_effects_dump.py 1536，2026-10-13）：
//   1536 row1201（圣骑·赫里斯 3938）args[3,2,3,2,1]（kind[0,5,14]，单行）
//     「自身使用攻击技能则战斗阶段结束时附加自身最大体力1/3的固定伤害，若当前体力低于
//     最大体力的1/2则效果翻倍；自身攻击命中后则战斗阶段结束时恢复自身最大体力的1/3，
//     若当前体力低于最大体力的1/2则效果翻倍（boss有效）」
//   → db[0]=附加分母（自身 max/db[0]）；db[1]=附加翻倍门（当前 < max/db[1]）；
//     db[2]=回复分母；db[3]=回复翻倍门；db[4]=1 不消费（K2 纪律记档）。
//     tips 为权威文（SOP §六.1）。**boss有效**：本行无 boss 免除，不做 boss 门（记档）。
//
// 实现口径：
//   - 双旗账本（plugin_storage，ROUND_END 读后清）：
//     使用旗 = EVENT_SKILL_USED watcher（own + !used_attribute——"使用攻击技能"，
//     E13 使用即触发，miss 也算）；命中旗 = EVENT_TAKE_DAMAGE watcher（K1 766 门：
//     ATTACK_DAMAGE 时点 + amount>0——"攻击命中后"）；
//   - ROUND_END：旗 A → deal_pink_damage(自身 max/db[0], FIXED)（对当前对手；
//     K233/K1 FIXED 档）；旗 B → heal_amount(max/db[2])；低体（当前 < max/db[1|3]，
//     各自门）→ ×2（"效果翻倍"）；
//   - 命中蕴含使用（B ⇒ A），miss/属性技双静默（属性技不走红伤管线且 used_attribute
//     门排除）；
//   - **boss有效**：默认无门即对 boss 生效（K190 忽略口径的原命题，记档）。
//
// 动工前置三查（2026-10-13，author k-prod-6 第三轮）：①内核 grep 1536 无魂印行为
//   硬编码；②effect_icon kind=[0,5,14]、tips 为权威文；③registry 无 1536、soul_lib
//   并集（hub+feat/k226-k249）无 1536——真缺口。
// 先例：K226/K239（SKILL_USED/TAKE_DAMAGE 双窗形）、K233 844（ROUND_END FIXED 附加）、
//   K226（ROUND_END heal）、K229（低体快照门+翻倍）。
// 场景回归：test/scenario/scenario_4000_usedhit_roundend_attach_heal_1536.cpp（号段 4000-4009）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1536;
constexpr int kWiredKeyBase = 991449000;     // +owner：watcher 已挂旗标
constexpr int kUsedAttackBase = 991450000;   // +owner：本回合使用攻击技旗
constexpr int kHitBase = 991451000;          // +owner：本回合攻击命中旗

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

// ── early：双旗 watcher（arm-once）──────────────────────────────────────────

EffectResult soulmark_1536_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    // 使用旗：own + 攻击技（E13 使用即触发）
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            wctx->plugin_storage[kUsedAttackBase + owner] = 1;
        },
        WatcherScope::TEAM);
    // 命中旗：own 攻击命中造成伤害（K1 766 门）
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
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
            wctx->plugin_storage[kHitBase + owner] = 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ROUND_END：旗 A → 附加（低体×2）；旗 B → 回复（低体×2）──────────────────

EffectResult soulmark_1536_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int attach_denom = db_at(row, 0, -1);
    const int attach_gate = db_at(row, 1, -1);
    const int heal_denom = db_at(row, 2, -1);
    const int heal_gate = db_at(row, 3, -1);
    if (attach_denom <= 0 || attach_gate <= 0 || heal_denom <= 0 || heal_gate <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0 || self.hp <= 0) {
        return EffectResult::kOk;
    }
    const bool low = self.hp < max_hp / attach_gate;   // db[1]/db[3] 同值同门
    const auto uit = ctx->plugin_storage.find(kUsedAttackBase + owner);
    if (uit != ctx->plugin_storage.end()) {
        ctx->plugin_storage.erase(uit);
        const int attach = max_hp / attach_denom * (low ? 2 : 1);
        if (attach > 0 && g_core && g_core->deal_pink_damage) {
            (void)g_core->deal_pink_damage(ctx, 1 - owner, attach,
                                           DamageKind::FIXED, owner);
        }
    }
    const auto hit = ctx->plugin_storage.find(kHitBase + owner);
    if (hit != ctx->plugin_storage.end()) {
        ctx->plugin_storage.erase(hit);
        const int heal = max_hp / heal_denom * (low ? 2 : 1);
        if (g_core && g_core->heal_amount) {
            (void)g_core->heal_amount(ctx, owner, heal);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1536）。early=双旗 watcher；ROUND_END=附加+回复（boss有效不设门）。
SOUL_PROGRAM(1536, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1536_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1536_round_end,
                    /*once=*/false, /*early=*/false)});
