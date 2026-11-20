// K363 用技类型先制/暴击窗族 —— effect 754（1 宏 / 2 行）
//（锦马超 3092；号段 7080-7089，worktree ../agents/k363，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 754，2026-10-24）：
//   row434 锦马超 3092 args[1,50,1]：
//     「使用攻击技能后下回合所有技能额外先制+1；使用属性技能后下回合攻击技能
//       致命一击概率提升50%（BOSS无效）」
//   row461 锦马超（伙伴效果）args[2,60,1]：同文，1→2、50→60。
//   → db[0]=下回合先制+N（1/2）；db[1]=下回合暴击率+M 百分点（50/60）；
//     db[2]=1 标志位（两行恒 1，不消费——K1 纪律）。
// ⚠️ 三查记录（author a22-kprod 2026-10-24）：①内核 grep \b754\b 无魂印行为硬编码；
//    ②effect_info 754 = 技能空间模板「消耗自身全部体力，使下只出场精灵前{0}回合
//      必定先手…」（异文，魂印以 effect_icon tips 为权威）；③registry soul 无 754、
//      soul_lib 全库 grep 无同 id 注册——真缺口。
// 实现（板行处方）：
//   「使用X技能后」判据 = 判据统一 §四点五：BATTLE_ROUND_END 节点读
//   executed_this_round（skill_use_seq 增量；被控/NONE 不算使用）+ skill_type_view
//   （物理/特殊=攻击技；Attribute=属性技——属性技无伤害阶段本就不参与暴击，side 级
//   暴击槽即"攻击技能"口径）。命中记 soulmark_storage 回合戳旗标（ws 每回合 reset，
//   跨回合锚必须放 soulmark_storage——K218Runtime 同款容器）。
//   ①下回合先制：BATTLE_FIRST_MOVE_RIGHT 桶节点读旗标（attack_round == roundCount-1）
//     → ws.preemptive_level[owner] += db[0]。pl 的 memset 在 MOVE_RIGHT 时点
//     （SOP §二.2），FMR 桶写即当回合生效；advanceRound 在上回合 ROUND_COMPLETION
//     末尾 → FMR 的 roundCount-1 恰为用技回合。"所有技能"按 K218 277/K266 366 定例
//     落地为回合级 pl 写（引擎无逐技先制通道，记档）。
//   ②下回合暴击窗：BATTLE_ROUND_START 桶节点读旗标（attr_round == roundCount-1）→
//     ws.crit_rate_add[owner] += db[1]（加算百分点通道，d5_D4/696 先例；rate =
//     base×mod + add，add>0 即 base=0 技能也能暴）。ws 每回合 reset → 窗恰为单回合，
//     ROUND_START 重写即"下回合"窗语义（must_crit 同套路）。
// 行参：db_at 原序读 owned_int_args（K279 口径）。
// 注册：SOUL_PROGRAM(754) 三节点（ROUND_END 记账 + FMR 先制 + ROUND_START 暴击窗），
// SOUL_PROGRAM 总数 +1（只增不改）；三节点统一 boss_invalid（tips 尾注 BOSS无效）。
// 场景回归：test/scenario/scenario_7080_type_preempt_crit_window.cpp（号段 7080-7089）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 754;
constexpr int kPreemptFallback = 1;   // db[0] 兜底（tips 字面 +1）
constexpr int kCritFallback = 50;     // db[1] 兜底（tips 字面 +50%）

// ── 跨回合旗标（soulmark_storage 容器，K218Runtime 同款）───────────────────
struct K363Runtime {
    int attack_round = -1;   // 最近一次"成功使用攻击技"的回合
    int attr_round = -1;     // 最近一次"成功使用属性技"的回合
};

K363Runtime& k363_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K363Runtime{};
    }
    return std::any_cast<K363Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279 同款）────────────────────
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

// 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h，k_hit_suppress_cond 同款）。
bool executed_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// ── 记账节点：ROUND_END 读"本回合成功用了什么类型"并记回合戳 ────────────────
EffectResult k363_754_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (!executed_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 被控/NONE 不算"使用"（判据统一 2026-10-03）
    }
    K363Runtime& rt = k363_runtime(ctx, owner);
    if (executed_is_attack(ctx, owner)) {
        rt.attack_round = ctx->roundCount;
    } else {
        rt.attr_round = ctx->roundCount;   // 属性技（SkillType::Attribute）
    }
    return EffectResult::kOk;
}

// ── ①下回合先制：FMR 桶（memset 后写，SOP §二.2）──────────────────────────
EffectResult k363_754_move_right(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K363Runtime& rt = k363_runtime(ctx, owner);
    // ⚠️ 哨兵 -1 会与首回合 roundCount-1=-1 相撞（首回合空窗泄露，2026-10-24 补）。
    if (rt.attack_round < 0 || rt.attack_round != ctx->roundCount - 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int preempt = db_at(row, 0, kPreemptFallback);
    if (preempt > 0) {
        ctx->ws.preemptive_level[owner] += preempt;
    }
    return EffectResult::kOk;
}

// ── ②下回合暴击窗：ROUND_START 桶重写（ws 每回合 reset → 单回合窗）──────────
EffectResult k363_754_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K363Runtime& rt = k363_runtime(ctx, owner);
    // ⚠️ 同上：哨兵 -1 与首回合 roundCount-1 相撞防护。
    if (rt.attr_round < 0 || rt.attr_round != ctx->roundCount - 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int crit = db_at(row, 1, kCritFallback);
    if (crit > 0) {
        ctx->ws.crit_rate_add[owner] += static_cast<float>(crit);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（754 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k363_754_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k363_754_move_right,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k363_754_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
