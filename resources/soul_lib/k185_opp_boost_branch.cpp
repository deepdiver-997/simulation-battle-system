// K185 对手强化态双分支族 —— effect 1148（1 宏 / 2 行）
//                                                                          （2026-10-07，worktree ../agents/k185）
//
// 行清单（tools/soul_effects_dump.py 1148，2026-10-07）：
//   1148 倾城·虞姬 3560 两行 args[60,1]（本体）/"霸王别姬伙伴效果"行 args[100,1]：
//     「回合开始时若对手处于能力提升状态，则当回合自身攻击消除对手能力提升状态；
//     回合开始时若对手不处于能力提升状态，则当回合自身攻击造成的伤害提高{db[0]}%
//     （boss有效）」
//   → db[0]=无强化分支的增伤百分比（60/100，伙伴行更高）；db[1]=boss有效旗（tips 尾注
//     "（boss有效）"= 无 boss 门，**不消费**，记档位）。
//
// 实现（SOUL_PROGRAM 两节点）：
//   ① OPERATION_ENTER_EXIT_STAGE arm：注册 EVENT_TAKE_DAMAGE watcher（TEAM 永久，
//     K10 637 同款）：ev.target==opp && ev.actor==own && 攻击伤害事件（K10
//     is_attack_damage_event 私副本）→ 读**本回合分支旗**（plugin_storage，
//     ROUND_START 每回合重写）==有强化 → clear_stat_boosts(opp)（808/1011 同原语；
//     幂等，首次后清 0 无副作用）。⚠️ 消除不掷骰、攻击照常结算（消除在伤害落地后
//     的 watcher 回调=AFTER_ACTION 收口，K1 时点口径——条件不回查、只消费事件快照）。
//   ② BATTLE_ROUND_START：评对手 has_boost（本体 ability_levels 六槽任一正等级，
//     F5 1472/962 同判据、负等级不算）→ 有：置旗=1（本回合攻击消强、**不增伤**）；
//     无：置旗=0 + ws.damage_add_pct[own] += db[0]（通用增伤·加算通道，F5 1472 同款；
//     ws 每回合 reset → 每回合重写无累积）。
//
// 三查记录（2026-10-07，立单时已核）：①内核 grep 1148 无魂印行为硬编码；②effect_info
//   同号为技能空间模板（魂印以 effect_icon tips 为权威）；③soul_lib 并集无 1148、
//   registry.soul 无 1148——真缺口。
// 先例：K10 637（TAKE_DAMAGE watcher+事件快照守卫）、K164 109（ENTER arm 常驻 watcher）、
//   F5 1472（damage_add_pct 通道+boost 判据）、chaos_sorensen 1011（clear_stat_boosts）。
// 场景回归：test/scenario/scenario_3170_opp_boost_branch.cpp（T3170-T3173）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1148BranchKey = 990185001;   // +owner：本回合分支旗（1=对手有强化→消强）

// ── 宿主行参数（各文件自治同款）────────────────────────────────────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
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
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

bool opponent_has_boost(const BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

int branch_flag(const BattleContext* ctx, int owner) {
    auto it = ctx->plugin_storage.find(k1148BranchKey + owner);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second, 0);
}

// ── ① arm：攻击命中 → 分支旗=有强化时消除对手强化 ───────────────────────────
EffectResult soulmark_1148_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            // ⚠️ ev.actor 可能 -1（无行动方事件）——范围守卫必须带（K3 thorn 同款）。
            if (!c || ev.target != 1 - owner || ev.actor != owner || ev.amount <= 0
                || !is_attack_damage_event(ev)) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 1148)
                || branch_flag(c, owner) != 1) {
                return;   // 仅"对手有强化"分支的回合消强
            }
            (void)g_core->clear_stat_boosts(c, 1 - owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ② 回合开始：评对手强化态 → 置旗 / 写增伤 ────────────────────────────────
EffectResult soulmark_1148_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 1148)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, args, 1148);
    if (opponent_has_boost(ctx, opp)) {
        ctx->plugin_storage[k1148BranchKey + owner] = int{1};
        return EffectResult::kOk;   // 有强化：当回合攻击消强，不增伤
    }
    ctx->plugin_storage[k1148BranchKey + owner] = int{0};
    const int amp = db_at(row, 0, 0);
    if (amp > 0) {
        ctx->ws.damage_add_pct[owner] += static_cast<float>(amp);   // 无强化：增伤
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（只增不改）────────────────
SOUL_PROGRAM(1148, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1148_arm),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START,
                                    &soulmark_1148_round_start)});
