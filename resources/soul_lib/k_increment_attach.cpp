// K348 攻击递增附加固伤族 —— effect 807（1 宏 / 1 行）
//（冥王·灭天截 3168 args[100,400,1]；号段 5100-5109，worktree k348，2026-10-22）
//
// 行清单（tools/soul_effects_dump.py 807，2026-10-22）：
//   「登场后，攻击技能附加100点固定伤害，每次使用攻击技能额外增加100点固定伤害，
//     最高附加400点固定伤害（BOSS无效）」
//   → db[0]=100=首档/每档增量、db[1]=400=上限、db[2]=1 废位不消费。
//   第 n 次攻击使用（本次上场计）附着 min(db[0]×n, db[1])。
//
// ⚠️ 三查记录（a15-kprod 2026-10-22）：
//   ① 内核 grep 807 无魂印行为硬编码；
//   ② effect_info 807 为技能空间异文（「附加对手上次造成伤害数值的固定伤害」），
//      不同空间；
//   ③ registry soul 与全 feat 分支 soul_lib 并集均无 807 注册——真缺口。
// 实现形态（套路目录 §一 递增类）：
//   计数容器 = pet.on_stage_storage（本次上场有效、下场不保留，官方默认口径）。
//   ① EVENT_SKILL_USED（own + 攻击技；used_attribute 过滤）→ 计数 +1（miss 也计次
//      = "每次使用"字面）。
//   ② DETECT（own 红伤落地，k197/k329 同型）→ ordinal = 当前计数 →
//      deal_pink_damage(对手, min(db[0]×ordinal, db[1]), FIXED, owner)。
//      ⚠️ 实测（场景 5100 首跑）：EVENT_SKILL_USED 在**本刀 DETECT 之前**已 drain
//      （R1 首次读到计数 1）——K1"晚 drain"在此路径不晚；故档位直接取计数，不再 +1。
//      miss 的本次无 DETECT，但计数已在事件面 +1 → 后续档位正确前移。
//   ⚠️ 行参一律 db_at 读 owned_int_args 原序；显式 0 = 关闸不 fallback。
// ⚠️ tips 带「（BOSS无效）」→ 两节点 boss_invalid=true。
// 场景回归：test/scenario/scenario_5100_increment_attach.cpp（号段 5100-5109）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 807;
constexpr int kCounterKey = 807;         // on_stage_storage 计数槽
constexpr int kArmKeyBase = 990807000;   // +owner：TEAM 管线已接线旗标

struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, int owner) {
    HostRow row;
    if (!ctx || owner < 0 || owner > 1) {
        return row;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kMarkId);
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int use_count(BattleContext* ctx, int owner) {
    return any_int_or(ctx->getPet(owner).on_stage_storage[kCounterKey], 0);
}

// ① 使用计数（own + 攻击技，miss 也计次）。
EffectResult k348_807_count(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kArmKeyBase + 100 + owner;   // 计数 watcher 接线旗标
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != owner || ev.used_attribute) {
                return;   // 只数自己的攻击技使用（属性技不计）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            ElfPet& pet = c->getPet(owner);
            const int n = any_int_or(pet.on_stage_storage[kCounterKey], 0);
            pet.on_stage_storage[kCounterKey] = n + 1;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ② 攻击红伤落地 → 按第 n 档附粉。
EffectResult k348_807_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kArmKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->deal_pink_damage || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const HostRow row = host_row(c, owner);
            const int step = db_at(row, 0, 100);
            const int cap = db_at(row, 1, 400);
            // 实测：EVENT_SKILL_USED 在本刀 DETECT 之前已 drain（R1 首次即读到 1）
            // → 计数已含本次使用，档位 = 计数本身（K1 的"晚 drain"在此路径不晚）。
            const int ordinal = use_count(c, owner);
            int amount = step * ordinal;
            if (cap > 0 && amount > cap) {
                amount = cap;
            }
            if (amount > 0) {
                (void)g_core->deal_pink_damage(c, c->opponent(owner), amount,
                                               DamageKind::FIXED, owner);
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（807 入账，只增不改）──────
// ⚠️ boss_invalid=true（tips 明写「（BOSS无效）」）——两个节点统一。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k348_807_count,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k348_807_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
