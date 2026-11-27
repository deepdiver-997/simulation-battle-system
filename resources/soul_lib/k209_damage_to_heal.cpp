// K209 受伤转化体力族 —— effect 326（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 326，2026-10-07）：
//   326 天邪龙王 2652 args[30, 0]：「30%的概率将本回合受到的伤害转化为自身体力（对
//     BOSS无效)」→ db[0]=转化概率（30）、db[1] 恒 0（不消费——记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：双节点——
//   ①TAKE_DAMAGE watcher（arm-once）：ev.target==owner 且 amount>0 → 受伤账本累加
//     （soulmark_storage：{账本回合号, 累计值}，回合号变更即清——轮界自清）。
//   ②BATTLE_ROUND_COMPLETION 节点：账本>0 → roll_percent(row(0,30))（掷点 293 同款
//     调用方自掷——申报通道仅异常族）→ heal_amount(min(账本, 已损))，清账本。
//   （对BOSS无效）尾注 → boss_invalid 置位。
//
// ⚠️ 三查记录（author k-prod-2 2026-10-07）：①内核无 326 魂印行为硬编码 ②effect_info
//    无 326 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：undying_1067/K199（TAKE_DAMAGE arm-once）、k_open_window_immune 261（RC 回血）。
// 场景回归：test/scenario/scenario_3500_damage_to_heal.cpp（号段 3500-3509）。

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


constexpr int kMarkId = 326;

struct K209Runtime {
    bool watcher_armed = false;
    int ledger_round = -1;
    int ledger = 0;
};

K209Runtime& k209_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K209Runtime{};
    }
    return std::any_cast<K209Runtime&>(storage[kMarkId]);
}

EffectResult k209_326_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K209Runtime& rt = k209_runtime(ctx, owner);
    if (rt.watcher_armed) {
        return EffectResult::kOk;
    }
    rt.watcher_armed = true;
    const HostRow row = host_row(ctx, a, 326);
    const int chance = db_at(row, 0, 30);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;   // 仅对手造成的实际伤害入账
            }
            K209Runtime& rt = k209_runtime(wctx, owner);
            if (rt.ledger_round != wctx->roundCount) {
                rt.ledger_round = wctx->roundCount;
                rt.ledger = 0;
            }
            rt.ledger += ev.amount;
        },
        WatcherScope::TEAM);
    // RC 节点（HAND 写进桶由更新器重注册）：掷点转化，账本清零。
    // 节点函数捕获不到 watcher 的 runtime 引用安全性 → 现取（同键 any）。
    (void)chance;
    return EffectResult::kOk;
}

// ── RC 节点：掷点 → 本回合受伤转化为体力 ──
EffectResult k209_326_round_completion(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    K209Runtime& rt = k209_runtime(ctx, owner);
    if (rt.ledger_round != ctx->roundCount || rt.ledger <= 0) {
        return EffectResult::kOk;   // 本回合未受伤
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int chance = db_at(row, 0, 30);
    const int taken = rt.ledger;
    rt.ledger_round = -1;
    rt.ledger = 0;
    if (!roll_soul_percent(*ctx, kMarkId, 0, owner, owner, chance)) {
        return EffectResult::kOk;   // 概率未触发（293 同款调用方自掷）
    }
    ElfPet& self = ctx->getPet(owner);
    const int deficit = resolve_pet_max_hp(self) - self.hp;
    if (deficit > 0) {
        (void)g_core->heal_amount(ctx, owner, taken < deficit ? taken : deficit);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（326 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k209_326_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_COMPLETION,
                        &k209_326_round_completion, /*once=*/false, /*early=*/false,
                        SoulScope::STAGE, /*boss_invalid=*/true)});
