// K190 出场低血免疫弹控窗+受击叠防族 —— effect 870（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 870，2026-10-07）：
//   870 天尊龙帝 3256 args[2, 1, 1]：「出场时若自身体力低于对手，则出场前2回合免疫并
//     反弹异常状态；每次受到伤害时双防+1，自身体力低于对手时提升效果翻倍（BOSS无效）」
//     / args[3, 1, 1]（出场前3回合，进化行同构互锚）
//     → db[0]=免疫弹控窗回合数（2/3）、db[1]/db[2]=1 旗（官方 tips 无对应子句，不消费
//       ——记档）。BOSS 无效：author 立单口径"boss旗忽略"，本单不置位（记档）。
//
// ⚠️ 实现形态（T4 类型化行参）：单 enter 节点双职责——
//   ①低血门（出场时 self.hp < opp.hp）→ **免疫并反弹异常**：191/1381 定版配对 =
//     Modern 层 ANOMALY 免疫票（grant_immunity, ImmunityTier::Modern）+
//     grant_reflect（rule_center 回弹票）；窗长 = db[0]（免疫票引擎到期；
//     回弹票随 RuleCenter 票生命周期）。出场门每次登场重判（切换重上场重新授予）。
//   ②受击叠防：EVENT_TAKE_DAMAGE watcher（K179/1067 arm-once 形态，TEAM 永久）——
//     ev.amount>0 → stat_change 防(槽2)+1、特防(槽3)+1；受击时 self.hp < opp.hp
//     → +2（翻倍，当次判）。自伤/反弹伤不叠（ev.actor==owner 门，1067 同门）。
//
// ⚠️ 三查记录（author 2026-10-07 / k-prod-2 复核同日）：①内核无 870 魂印行为硬编码
//    ②effect_info 无 870 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：anomaly_channel 191（免疫+弹控定版配对）、k_open_window_immune 221（enter 免疫窗）、
//   undying_1067/K179（TAKE_DAMAGE watcher arm-once）。
// 场景回归：test/scenario/scenario_3220_enter_immune_stack.cpp（号段 3220-3229）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

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


constexpr int kMarkId = 870;

struct K190Runtime {
    bool watcher_armed = false;   // 叠防 watcher 只 arm 一次（切换不丢）
};

K190Runtime& k190_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K190Runtime{};
    }
    return std::any_cast<K190Runtime&>(storage[kMarkId]);
}

EffectResult k190_870_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const int opp = ctx->opponent(owner);
    ElfPet& self = ctx->getPet(owner);
    ElfPet& foe = ctx->getPet(opp);

    // ① 出场低血门 → 免疫并反弹异常（191 定版配对，窗长 db[0]）。
    if (self.hp > 0 && foe.hp > 0 && self.hp < foe.hp) {
        const HostRow row = host_row(ctx, a, 870);
        const int rounds = db_at(row, 0, 2);
        if (rounds > 0) {
            ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/rounds,
                                /*source_id=*/kMarkId, /*soul_immunity=*/false,
                                EffectScope::ON_STAGE, /*counts=*/0,
                                /*source_effect_id=*/kMarkId, ImmunityTier::Modern);
            ctx->rule_center_.grant_reflect(owner, /*source_slot=*/-1,
                                            /*source_effect_id=*/kMarkId,
                                            /*target=*/owner, /*rounds=*/rounds);
        }
    }

    // ② 受击叠防 watcher（arm 一次；ev 快照读伤害，翻倍当次判）。
    K190Runtime& rt = k190_runtime(ctx, owner);
    if (!rt.watcher_armed) {
        rt.watcher_armed = true;
        ctx->event_center_.register_watcher(
            EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || !g_core || !g_core->stat_change) {
                    return;
                }
                if (ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                    return;   // 仅对手出手且确有伤害（1067 同门 + 快照读数）
                }
                if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                    return;
                }
                ElfPet& s = wctx->getPet(owner);
                ElfPet& f = wctx->getPet(wctx->opponent(owner));
                const int delta = (s.hp > 0 && f.hp > 0 && s.hp < f.hp) ? 2 : 1;
                (void)g_core->stat_change(wctx, owner, /*stat=*/2, delta);   // 防御
                (void)g_core->stat_change(wctx, owner, /*stat=*/3, delta);   // 特防
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（870 入账，只增不改）────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k190_870_enter)});
