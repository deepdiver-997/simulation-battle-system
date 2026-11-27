// K375 异常门随机降属+护盾仅1次族 —— effect 385（1 宏 / 2 行）
//（战神·盖亚 2780 魂印「袭」row160 本体 + row458 伙伴行；号段 7220-7229，
//  worktree ../agents/k375，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 385，2026-10-24）：
//   row160 args[3,2,1,3,300] kind[2,12]：
//     「若自身不处于异常状态，则对手每回合随机两项属性－1。
//       若自身处于异常状态，则获得一个可抵御300点伤害的护盾（仅1次）」
//   row458 args[3,2,1,3,300,50,50]（战神兄弟伙伴效果）：同文 + 「攻击+50，体力+50」。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a76-jprod 动工同日复核）：
//   ①内核 grep \b385\b 无魂印行为硬编码；②effect_info 无 385 行（tips 权威）；
//   ③registry soul 无 385、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行前五位同构、row458 尾两位=tips 字面 50/50 自证）：
//   db[1]=项数（2，"随机两项"）、db[2]=降幅（1，"－1"）、db[4]=护盾量（300）；
//   db[0]=3 / db[3]=3：无 tips 锚（三行位全 3），记档不猜（K265 "解析不了宁可不用"）；
//   db[5]=攻击+50、db[6]=体力+50（仅 row458，size>=7 护栏）。
//
// 实现（板行处方）：
//   ① ROUND_START 异常门（has_any_active_abnormal_status——套装线 447 同款判据）：
//     无异常 → 随机去重 db[1] 项 stat_drop(对手, 槽, db[2])（k893 picked[] guard 循环）；
//     有异常 → 首次时 shield_bank_.add(kSoulShieldPriority, db[4], source=385,
//     duration=0 破盾前持续)（artemis 4745 魂印盾同款登记面）；"仅1次" =
//     soulmark_storage once 旗（授予即置，破盾/耗尽不复活）。
//   ② 伙伴行面板：SOUL_PRE_BATTLE(385, STAT_MODIFY)（wei_yue 4510 同通道）——
//     size>=7 护栏（本体行 5 参不落笔）→ 物攻+db[5]、体力面板+db[6]。
// 注册：SOUL_PROGRAM(385) 单节点 + SOUL_PRE_BATTLE(385) 一补丁；
// SOUL_PROGRAM 总数 +1（只增不改）。tips 无 boss 尾注 → 节点 boss_invalid=false
// （boss 挑战照常，K300 口径）。
// 场景回归：test/scenario/scenario_7220_anomaly_gate_shield_once.cpp（7220-7229）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/shield_bank.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 385;
constexpr int kShieldSource = 385;   // 护盾来源标识（定位/刷新用）
constexpr int kStatSlotCount = 6;

// ── "仅1次"旗（soulmark_storage 容器，K218/K363 同款）─────────────────────────
struct K375Runtime {
    bool shield_granted = false;
};

K375Runtime& k375_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K375Runtime{};
    }
    return std::any_cast<K375Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K363 同款）──────────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
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

// ── ① ROUND_START：异常门分叉 ────────────────────────────────────────────────
EffectResult k375_385_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);

    if (!ctx->has_any_active_abnormal_status(owner)) {
        // 无异常：对手每回合随机去重 db[1] 项 -db[2]（k893 循环，±6 原语内钳）。
        const int drop_count = db_at(row, 1, 0);
        const int drop_amount = db_at(row, 2, 1);
        if (drop_count > 0 && drop_amount != 0 && g_core && g_core->stat_drop &&
            ctx->getPet(opp).hp > 0) {
            int picked[kStatSlotCount] = {};
            int done = 0;
            int guard = 0;
            while (done < drop_count && guard < 100) {
                ++guard;
                const int slot = ctx->rng().next() % kStatSlotCount;
                if (picked[slot]) {
                    continue;
                }
                picked[slot] = 1;
                ++done;
                (void)g_core->stat_drop(ctx, opp, slot, drop_amount, owner);
            }
        }
        return EffectResult::kOk;
    }

    // 有异常：获得护盾（仅1次——once 旗置位后不再授予，破盾/耗尽不复活）。
    K375Runtime& rt = k375_runtime(ctx, owner);
    const int shield_qty = db_at(row, 4, 0);
    if (!rt.shield_granted && shield_qty > 0) {
        ctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority, shield_qty,
                                            kShieldSource, /*duration=*/0);
        rt.shield_granted = true;
    }
    return EffectResult::kOk;
}

// ── ② 伙伴行面板（STAT_MODIFY，size>=7 护栏——本体行 5 参不落笔）─────────────
void k375_385_panel(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot >= 6) {
        return;
    }
    ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
    if (pet.soulMark.id != kMarkId) {
        return;
    }
    const auto& db = pet.soulMark.args.owned_int_args;
    if (db.size() < 7) {
        return;   // 本体行 row160（5 参）无面板位
    }
    if (db[5] > 0) {
        pet.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK] += db[5];
    }
    if (db[6] > 0) {
        pet.numericalProperties[NumericalPropertyIndex::HP] += db[6];
    }
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（385 入账，只增不改）──────
// tips 无 boss 尾注 → boss_invalid=false（boss 挑战照常）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k375_385_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
SOUL_PRE_BATTLE(kMarkId, PreBattlePhase::STAT_MODIFY, k375_385_panel);
