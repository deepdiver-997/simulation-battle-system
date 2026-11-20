// K261 低血门减伤+后手加速族 —— effect 373（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 373，2026-10-21）：
//   373 神剑·天音 2792 args[1,150,5,1]（kind=[3,11]）：「回合开始时若血量低于对手则
//     受到的伤害减少150点；后出手时速度+1」
//     → **db[1] = 减伤点数 150**（tips 字面，与 db[1] 一致）；db[0]=1 / db[2]=5 /
//       db[3]=1 语义存疑不消费、记档——注意 **db[2]=5 与速度槽号 4 不符**，
//       db[3]=1 与 320 的 db[4]=1 位置也不同（K4 头注："k_allowed 布局互不通用"）。
//       按 K2 纪律（解析不了宁可不用、不猜）：数值一律取 tips 字面
//       （150 点 / 速度+1），db 布局原样记档备查。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   子句①「回合开始时若血量低于对手则受到的伤害减少150点」
//     - 门 = **回合开始时**估"自身体力 < 对手体力"（官方锚"回合开始时"，
//       K229 1422 同款快照口径）→ ROUND_START 节点写账本旗（plugin_storage，
//       K7 账本显式写/清模式）；减伤票**只读旗、不重估**——回合内 HP 反超不改
//       本回合档位（K229 记档口径）。
//     - 票 = DamagePhase::REDUCE_FLAT + DamageEffectCategory::MITIGATE
//       （**固定点数**减伤，非百分比——K1 329 / K 板 337「伤害减少N点」同款）；
//       只红伤（"受到的伤害"按 K184/K229/K235 家族口径 = 红伤票，粉伤不沾，记档）。
//       减到 0 下限（std::max(0, ...)）。
//     - 票挂在 arm 节点（early 幂等旗），回调里**消费时点直读**账本旗。
//   子句②「后出手时速度+1」
//     - 门 = ctx->is_second_mover(owner)（引擎 helper：先手权由 MOVE_RIGHT 结算写入
//       ws.preemptive_right）——⚠️ **时点必须在 MOVE_RIGHT 结算之后**：K235 705 实测
//       记档「FMR 桶内读恒 NONE」（handle_BattleFirstMoveRight 入口清零、桶后结算），
//       ROUND_START 更早（MOVE_RIGHT 之前）→ 读到 NONE 一律判"先手"。
//     - ⚠️ **本夜实测（2026-10-21 a01-kprod，K261）**：BATTLE_*_ACTION_START 这个
//       State 挂魂印节点**不会被执行**——内核 `execute_registered_actions(
//       first_mover_id / second_mover_id, State)` 按**行动方 id** 索引桶，而魂印
//       节点注册到**持印者**桶，两者只在"持印者恰好是当前行动方"时重合；实测
//       持印者后手时该节点零调用（探针槽位始终 -1）。
//       探针同时确认 **BATTLE_ROUND_END 桶会跑且此时 pr 已定**（is_second_mover
//       读值正确）→ 本句挂 ROUND_END。⚠️ 口径记档：回合末生效 → 加速作用于
//       **下一回合**（本回合出手序已定，速度改动不影响已发生的先手比较）。
//     - 效果 = stat_change(owner, 速度槽 4, +1)（自身增益通道，不查免弱；
//       K255 320 / K196 390 同款）。⚠️ 等级上限 ±6 由原语内钳。
//   ⚠️ boss 门：tips 无（boss无效/免疫）尾注 → 节点 boss_invalid 不置位。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 373 无魂印行为硬编码；
//    ②effect_icon kind=[3,11]、tips 为权威文（dump 原文已核，effect_info 无 373
//    技能模板）；③registry soul 集合无 373、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k229_hpcmp_mitigate_drain 1422（体力对照门 + 账本 + REDUCE_PCT 票）、
//   k_guard_family 329（REDUCE_FLAT 固定点数减伤票）、k235 705（is_second_mover
//   消费时点直读）、K255 320（stat_change 自增益）。
// 场景回归：test/scenario/scenario_4170_lowhp_flat_speed_373.cpp（号段 4170-4179）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 373;
constexpr int kReduceFallback = 150;   // tips 字面减伤点数（db[1] 读不到时回退）
constexpr int kSpeedSlot = kAbilityLevelIndexSpeed;   // 4（速度等级槽）
constexpr int kLowHpFlagBase = 991473000;   // +owner：本回合"血量低于对手"旗（1/0）

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

int flag_get(BattleContext* ctx, int owner) {
    const auto it = ctx->plugin_storage.find(kLowHpFlagBase + owner);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second, 0);
}

// arm：挂 REDUCE_FLAT 减伤票（读账本旗，不重估）。
EffectResult k261_373_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = 990261000 + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    const HostRow row = host_row(ctx, a, kMarkId);
    const int amount = db_at(row, 1, kReduceFallback);
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [owner, amount](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMarkId)) {
                return;
            }
            if (flag_get(c, bucket_owner) != 1) {
                return;   // 回合开始时未处于"血量低于对手" → 本回合不减免
            }
            d.final = std::max(0, d.final - amount);
        });
    return EffectResult::kOk;
}

// ROUND_START：估"血量低于对手"写旗。
EffectResult k261_373_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // 子句①：回合开始时估"自身体力 < 对手体力" → 写旗（回合内不重估）。
    const int opp = ctx->opponent(owner);
    const int self_hp = ctx->getPet(owner).hp;
    const int opp_hp = ctx->getPet(opp).hp;
    ctx->plugin_storage[kLowHpFlagBase + owner] = (self_hp < opp_hp) ? 1 : 0;
    return EffectResult::kOk;
}

// AFTER_ACTION（先手权已定）：后手方速度+1。
EffectResult k261_373_after_action(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (!ctx->is_second_mover(owner)) {
        return EffectResult::kOk;   // 本方是先手 → 不加速
    }
    (void)g_core->stat_change(ctx, owner, kSpeedSlot, /*delta=*/1);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（373 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k261_373_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_START, &k261_373_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_END, &k261_373_after_action,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
