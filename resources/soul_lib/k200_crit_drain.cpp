// K200 暴击吸血族 —— effect 321（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 321，2026-10-07）：
//   321 女神·雅典娜 2661 args[30, 0]：「若打出致命一击，则造成伤害值的30%恢复自身体力」
//     → db[0]=恢复比例（30）、db[1] 恒 0（官方 tips 无对应子句，不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：EVENT_TAKE_DAMAGE watcher（K179 arm-once 形态；事件面
//    无桶归属限制——K2 262 TAKE_DAMAGE watcher 形，对手受击也收得到）：
//    ev.target==opp 且 ev.actor==owner 且 ev.amount>0 且 wctx->crit_happened[owner]
//    （暴击旗：一次技能只掷一次，受击时点现行值）→ heal_amount(owner, amount×db[0]/100)。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 321 魂印行为硬编码 ②effect_info
//    无 321 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：undying_1067/K179（TAKE_DAMAGE watcher arm-once）、yanshou（heal_amount）。
// 场景回归：test/scenario/scenario_3360_crit_drain.cpp（号段 3360-3369）。

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


constexpr int kMarkId = 321;

struct K200Runtime {
    bool watcher_armed = false;
};

K200Runtime& k200_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K200Runtime{};
    }
    return std::any_cast<K200Runtime&>(storage[kMarkId]);
}

EffectResult k200_321_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K200Runtime& rt = k200_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    // 行参走类型化 SoulNodeArgs（T4）：row(0)=恢复比例。
    const HostRow row = host_row(ctx, a, 321);
    const int pct = db_at(row, 0, 30);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || !g_core || !g_core->heal_amount) {
                return;
            }
            const int opp = wctx->opponent(owner);
            if (ev.target != opp || ev.actor != owner || ev.amount <= 0) {
                return;   // 仅自己的攻击对对手造成实际伤害
            }
            if (!wctx->crit_happened[owner]) {
                return;   // 暴击门（掷点现行值，一次技能一掷）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            (void)g_core->heal_amount(wctx, owner, ev.amount * pct / 100);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（321 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k200_321_arm,
                        /*once=*/false, /*early=*/true)});
