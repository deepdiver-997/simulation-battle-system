// K163 登场免疫恢复族 —— effect 270/261（2 宏 / 4 行）
// 221 已收敛到 k_death_revenge_family.cpp 的 early 登场节点（官方 2 行，args 空）。
//
// 行清单（tools/soul_effects_dump.py 270 261，2026-10-05）：
//   270 魔眼凯兮 2521 args[0,2] / 斯凯尔托斯 2476 args[]（2 行）：
//     "登场2回合内不会陷入能力下降状态" → enter 节点 grant_immunity(STAT_DROP,
//     duration=db[1] or 2)（528 强化保护窗同款；stat_drop 原语查 STAT_DROP 免弱）。
//     db[0]=0 语义不明不消费（K1 纪律）。
//   261 格洛 2430 / 格洛斯 2431（2 行，args[]）：
//     "每回合恢复1/10最大体力" → ROUND_START 节点 heal_amount(max/10)
//     （K4 622/636 ROUND_START 回合结算直系）。
//
// ⚠️ 实现形态：全部**声明进 SOUL_PROGRAM**（更新器每回合重注册；本族无事件面需求、
//    无 watcher）。STAGE scope：宿主不在场时更新器不注册（register_soul_effect 的
//    owner_on_stage 门），离场即失效——符合"出战时/登场时"字面。
//
// ⚠️ 三查记录（2026-10-05）：①内核 grep 221/270/261 无魂印行为硬编码 ②effect_info
//    无此三 id 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：K6 k_hit_suppress_cond（SOUL_PROGRAM 声明式节点形态）、481/471/528（免疫窗）、
//   K4 622/636（ROUND_START 回合结算）。
// 场景回归：test/scenario/scenario_2880_open_window_immune.cpp（号段 2880-2889）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 宿主行解析（K2 host_row 私副本：owner+slot+mark args；hand-built 标记 args 空→全 fallback）。
struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const std::vector<int>* db, int idx, int fallback) {
    if (db && idx >= 0 && idx < static_cast<int>(db->size())) {
        return (*db)[idx];
    }
    return fallback;
}

// ── 270 enter 节点：登场 2 回合免降 ──
EffectResult k163_270_enter(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 270);
    const int rounds = db_at(row.db, 1, 2);
    if (rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(),
                        /*anomaly_mask=*/0, /*duration_rounds=*/rounds, /*source_id=*/270,
                        /*soul_immunity=*/true, EffectScope::ON_STAGE, /*counts=*/0,
                        /*source_effect_id=*/270);
    return EffectResult::kOk;
}

// ── 261 ROUND_START 节点：每回合回 max/10 ──
EffectResult k163_261_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 261)) {
        return EffectResult::kOk;
    }
    const int max_hp = ctx->seerRobot[owner].elfPets[ctx->on_stage[owner]].numericalBase[
        NumericalPropertyIndex::HP];
    if (max_hp > 0) {
        (void)g_core->heal_amount(ctx, owner, max_hp / 10);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）：保留 270 / 261，221 仅留 K11 early 节点 ───────────
SOUL_PROGRAM(270, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k163_270_enter)});
SOUL_PROGRAM(261, {SoulMarkNodeRef(State::BATTLE_ROUND_START, &k163_261_round_start)});
