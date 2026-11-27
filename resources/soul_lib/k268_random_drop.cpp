// K268 随机对手降能力族 —— effect 796（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 796，2026-10-21）：
//   796 千煞魔星 3153 args[2,1]（kind=[12]）：「每回合使对手随机2项能力-1」
//     → **db[0] = 随机项数 2**（tips 字面 "随机2项" ↔ 2）；**db[1] = 降属幅度 1**
//       （tips 字面 "-1" ↔ 1）。两参数与文本**完全自洽**（本夜少见的自洽行，
//       对照 382 382 的 db 布局无法自证）→ 两者均消费。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = **BATTLE_ROUND_START**（"每回合"——每回合一次，K229 1422 / K 板 359
//     同款回合首节点；声明节点每回合由更新器重注册，SOP §一.1）。
//   效果 = 随机 db[0] 个**互不重复**的能力槽 → stat_drop(对手, 槽, db[1], 印主)
//     ——**弱化原语**（查附加禁令 ATTACH_BAN + 免弱 STAT_DROP + 钳 -6；
//     K 板 893 随机降属同款：`stat_drop(wctx, opp, slot, drop_amount, owner)`）。
//     ⚠️ 选槽去重：k893 用 picked[6] 去重循环（避免同槽重复降），本单同款。
//     ⚠️ 对手已倒下场时 stat_drop 无存活守卫（k893 记档口径）——本单加存活守卫
//     （对手 hp<=0 则不降），记档为口径选择。
//   ⚠️ 官方"能力"6 槽 = 0攻 1特攻 2防 3特防 4速 5命中（常识速查 §三）——与
//     k893 的 sbs_rand()%6 覆盖范围一致。
//
// ⚠️ 三查记录（author a06-kprod 2026-10-21）：①内核 grep 796 无魂印行为硬编码
//    （moves_lib 的 796 是**技能空间**同号：j_drain_window3 "796 [rounds,分母]
//    {0}回合内每回合吸取对手当前体力的1/{1}"——不同注册空间，不算已实现）；
//    ②effect_icon kind=[12]、tips 为权威文（dump 原文已核，effect_info 无 796
//    技能模板）；③registry soul 集合无 796、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k893_random_drop（随机去重选槽 + stat_drop 同型，旗舰）、k229 1422
//   （ROUND_START 回合首节点）、k 板 370/682（随机项+对应增益同族）。
// 场景回归：test/scenario/scenario_4240_random_drop_796.cpp（号段 4240-4249）。

#include "common.h"

#include <cstdlib>

namespace {

constexpr int kMarkId = 796;
constexpr int kCountFallback = 2;    // tips 字面"随机2项"（db[0] 读不到时回退）
constexpr int kAmountFallback = 1;   // tips 字面"-1"（db[1] 读不到时回退）

// 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）。
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

// ROUND_START：使对手随机 db[0] 项能力 -db[1]。
EffectResult k268_796_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int count = db_at(row, 0, kCountFallback);
    const int amount = db_at(row, 1, kAmountFallback);
    if (count <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：不降（口径选择，记档）
    }
    // 随机去重选槽（k893 同款：picked[6] 避免同槽重复降）。
    int picked[6] = {};
    int done = 0;
    int guard = 0;
    while (done < count && guard < 100) {
        ++guard;
        const int slot = choose_soul_range(*ctx, kMarkId, 0, owner, opp, 0, 5);
        if (picked[slot]) {
            continue;
        }
        picked[slot] = 1;
        ++done;
        (void)g_core->stat_drop(ctx, opp, slot, amount, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（796 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k268_796_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
