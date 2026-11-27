// K282 回合随机属性+1族 —— effect 645（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 645，2026-10-21）：
//   645 月光之龙 2908 args[2,1]（kind=[11]）：「每回合随机2项属性+1」
//     → **db[0] = 随机项数 2**（tips 字面 "随机2项" ↔ 2）；**db[1] = 提升等级 1**
//       （tips 字面 "+1" ↔ 1）。两参数与文本**完全自洽** → 均消费。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = **BATTLE_ROUND_START**（"每回合"——K268 796 / K 板 359 同款回合首
//     节点；声明节点每回合由更新器重注册，SOP §一.1）。
//   效果 = 随机 db[0] 个**互不重复**的能力槽 → stat_change(印主, 槽, db[1])
//     ——**自身增益通道**（**不查免弱**：stat_drop 才查免弱；K 板 622「每回合使自身
//     随机属性+1」同款写法：`stat_change(ctx, owner, sbs_rand()%6, 1)`）。
//     ⚠️ 与 K268 796 的差别：796 是**降**对手（stat_drop，查免弱/附加禁令/钳 -6），
//       本单是**升**自己（stat_change，不查免弱、原语内钳 ±6）。
//   选槽复用 attach_random_anomalies 原语的部分 Fisher-Yates 套路：count 钳到 0..6，
//   不预筛 +6 槽，提升仍走 stat_change 内钳。2026-11-06 收敛 K267 重复注册。
//   RNG 消费变化：旧拒绝采样最多 100 次且次数随碰撞变化；现每个选槽恰消费一次，
//   包括最后剩 1 槽时的 %1；count<=0 / delta==0 均不消费。不保留旧选槽 golden。
//   ⚠️ 官方"属性"6 槽 = 0攻 1特攻 2防 3特防 4速 5命中（常识速查 §三）。
//
// ⚠️ 三查记录（author a10-kprod 2026-10-21）：①内核 grep 645 无魂印行为硬编码
//    ——moves_lib 的 645 是**技能空间**同号：effect_batch8「645 体力低于1/{0}时
//    威力{1}倍（玄铁重盾 16330 等，52 技）」，**同号异实**、不同注册空间，不算
//    魂印已实现（记档）；②effect_icon kind=[11]、tips 为权威文（dump 原文已核，
//    effect_info 无 645 技能模板）；③registry soul 集合无 645、soul_lib 全库
//    grep 无同 id 注册——真缺口。
// 先例：K268 796（随机去重选槽 + 弱化直系，旗舰）、k893_random_drop（随机降属）、
//   k_round_stat 622（随机槽 stat_change +1 同款）。
// 场景回归：test/scenario/scenario_4380_random_stat_645.cpp（号段 4380-4389）。

#include "common.h"

#include <algorithm>
#include <array>

namespace {

constexpr int kMarkId = 645;
constexpr int kCountFallback = 2;   // tips 字面"随机2项"（db[0] 读不到时回退）
constexpr int kDeltaFallback = 1;   // tips 字面"+1"（db[1] 读不到时回退）

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

// ROUND_START：自身随机 db[0] 项能力 +db[1]。
EffectResult k282_645_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    constexpr int total = BattleContext::kAbilityLevelSlotCount;
    const int count = std::clamp(db_at(row, 0, kCountFallback), 0, total);
    const int delta = db_at(row, 1, kDeltaFallback);
    if (count == 0 || delta == 0) {
        return EffectResult::kOk;
    }
    // 部分 Fisher-Yates：全六槽入袋（含命中和已 +6 槽），每次从剩余槽中取一项。
    std::array<int, total> bag{};
    for (int i = 0; i < total; ++i) {
        bag[i] = i;
    }
    for (int pick = 0; pick < count; ++pick) {
        const int idx = pick + static_cast<int>(ctx->rng().next() % (total - pick));
        std::swap(bag[pick], bag[idx]);
        (void)g_core->stat_change(ctx, owner, bag[pick], delta);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 唯一注册（645；K267/K282 同行实现已收敛，保留唯一ID覆盖）─────────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k282_645_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
