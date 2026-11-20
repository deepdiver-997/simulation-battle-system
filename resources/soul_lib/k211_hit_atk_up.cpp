// K211 受击概率攻升族 —— effect 315（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 315，2026-10-07）：
//   315 乱斗骑士 2642 args[20, 1, 1]：「受到攻击有20%概率使自身攻击+1（boss有效）」
//     → db[0]=概率（20）、db[1]/db[2]=1 旗（官方 tips 无对应子句，不消费——记档）。
//     boss有效 → 不置 boss_invalid。
//
// ⚠️ 实现形态（T4 类型化行参）：EVENT_TAKE_DAMAGE watcher（K199 直系，arm-once）：
//    ev.target==owner 且 ev.actor!=owner 且 ev.amount>0 → roll_percent(db[0])
//    （调用方自掷，293 同款）→ stat_change(owner, 攻槽0, +1)。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 315 魂印行为硬编码 ②effect_info
//    无 315 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：k199_hit_clear_boost（TAKE_DAMAGE watcher + 现值口径全形态）。
// 场景回归：test/scenario/scenario_3530_hit_atk_up.cpp（号段 3530-3539）。

#include "common.h"

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
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


constexpr int kMarkId = 315;

struct K211Runtime {
    bool watcher_armed = false;
};

K211Runtime& k211_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K211Runtime{};
    }
    return std::any_cast<K211Runtime&>(storage[kMarkId]);
}

EffectResult k211_315_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K211Runtime& rt = k211_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const HostRow row = host_row(ctx, a, 315);
    const int chance = db_at(row, 0, 20);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, chance](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->stat_change) {
                return;
            }
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;   // 仅对手造成的实际伤害（现值口径，K199 同门）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            if (!roll_percent(chance)) {
                return;
            }
            (void)g_core->stat_change(wctx, owner, /*stat=*/0, 1);   // 攻击 +1
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（315 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k211_315_arm,
                        /*once=*/false, /*early=*/true)});
