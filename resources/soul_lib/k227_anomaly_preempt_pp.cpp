// K227 · 异常先制+PP一降一升+全零追击族 —— effect 2011（1 宏 2 行）
//                                                                          （2026-10-08，day 专项 worktree ../agents/k227）
//
// 行清单（tools/soul_effects_dump.py 2011，2026-10-08）：
//   2011 row1625/1993（赤虹仙凤 4407，本体/进化双行）args[2,1,1,1]/[2,1,2,1]
//     「回合开始时若双方任意一方处于异常状态则当回合自身所有技能额外先制+2；|
//     战斗阶段结束时降低对手所有技能1点PP值并恢复自身所有技能N点PP值，之后若对手的
//     所有技能PP值均为0则下回合自身使用技能后额外消耗对手全部体力|（boss无效）」
//   → db[0]=先制增量（恒2）；db[1]=对手 PP 降量（恒1）；db[2]=自身 PP 回复量
//     （1/2，行间唯一变量）；db[3]=1 不消费（K2 纪律：解析不了的字段宁可不用，不猜）。
//   ⚠ effect_info 2011 模板（"持有北冥狂刃…"）为 moves 空间错位文——soul 侧以 tips 为
//     权威（SOP §六.1）。
//
// 实现（四节点，k173 先制旗/FMR 消费同构；（boss无效）→ 全节点标 boss_invalid）：
//   ① BATTLE_ROUND_START：双方任一有活跃异常（g_core->has_active_anomaly 全池扫，
//     crawford blocked_by_control 同款 0..kOfficialAbnormalStatusMaxId）→ 置先制旗；
//     同点惰性清过期追击旗（旗值 < 本回合号 = "下回合"机会已过，官方字面口径）。
//   ② BATTLE_FIRST_MOVE_RIGHT：消费先制旗 → ws.preemptive_level[owner]+=db[0]
//     （k173 ④/diwang_3512 同款——FMR 桶在 preemptive memset 之后跑 → 当回合生效）。
//   ③ BATTLE_ROUND_END（=官方"战斗阶段结束时"，chaos_sorensen 等五处同锚）：
//     对手全技能 PP-db[1]（地板0）→ 自身全技能 PP+db[2]（crawford 封顶 maxPP 惯例）
//     → 若对手 maxPP>0 槽全部 pp==0 → 置追击旗（旗值 = 下回合号；全零是吸收态，
//     之后每回合末都会重挂——官方逐回合复查口径）。
//   ④ EVENT_SKILL_USED watcher（OPERATION_ENTER_EXIT_STAGE early 挂 TEAM，662/K164
//     同窗；无 used_attribute 过滤——"使用技能"含属性技）：旗值==当前回合号 → 清旗 →
//     force_hp_to_zero(opp)（存活门，K164 151 同款）。
//
// 动工前置三查（2026-10-08）：①内核 grep 2011 无硬编码（0 命中）；②effect_icon
//   kind=[1,15,18]、effect_info 模板系 moves 空间错位文（以 tips 为权威）；③registry
//   soul=173 无 2011、soul_lib 无注册。
// 先例：k173（先制旗+FMR 消费+boss_invalid 全节点）、K2/K164（EVENT_SKILL_USED 窗）、
//   crawford（has_active_anomaly 全池扫 + PP 封顶回复）、K164 151（force_hp_to_zero+存活门）。
// 场景回归：test/scenario/scenario_3710_anomaly_preempt_pp.cpp（号段 3710-3719）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 2011;
constexpr int kPreemptFlagBase = 990227000;   // +owner：先制旗（int 1）
constexpr int kDrainFlagBase = 990227100;     // +owner：追击旗（int = 目标回合号）

// ── 宿主行参数（"行≠宏"：全量 DB args + monster_id，K2/K164 同款私副本）──────

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

bool watcher_wired(BattleContext* ctx, int owner) {
    const int key = 990227200 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// "处于异常状态" = 任一官方异常槽活跃（crawford 全池扫同款，无控/限类过滤——
// tips 只说"异常状态"不过滤子类）。
bool has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

// ── ① ROUND_START：异常门 → 先制旗（并惰性清过期追击旗）─────────────────────

EffectResult soulmark_2011_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int drain_flag_key = kDrainFlagBase + owner;
    const auto stale = ctx->plugin_storage.find(drain_flag_key);
    if (stale != ctx->plugin_storage.end()
        && std::any_cast<int>(stale->second) < ctx->roundCount) {
        ctx->plugin_storage.erase(stale);   // "下回合"机会已过（未用技）
    }
    if (!onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    if (has_any_anomaly(ctx, owner) || has_any_anomaly(ctx, 1 - owner)) {
        ctx->plugin_storage[kPreemptFlagBase + owner] = 1;
    }
    return EffectResult::kOk;
}

// ── ② FMR：消费先制旗 → 当回合先制+db[0] ────────────────────────────────────

EffectResult soulmark_2011_fmr(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const int key = kPreemptFlagBase + owner;
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(it);
    const HostRow row = host_row(ctx, args, kMark);
    const int bonus = db_at(row, 0, 2);
    if (bonus > 0) {
        ctx->ws.preemptive_level[owner] += bonus;
    }
    return EffectResult::kOk;
}

// ── ③ ROUND_END：对手 PP-1 / 自身 PP+db[2] → 全零则挂追击旗（下回合号）──────

EffectResult soulmark_2011_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int pp_down = db_at(row, 1, 1);
    const int pp_up = db_at(row, 2, 1);
    if (pp_down <= 0 && pp_up <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& opp = ctx->getPet(1 - owner);
    ElfPet& self = ctx->getPet(owner);
    for (Skills& s : opp.skills) {
        if (pp_down > 0 && s.pp > 0) {
            s.pp -= pp_down;
            if (s.pp < 0) {
                s.pp = 0;
            }
        }
    }
    for (Skills& s : self.skills) {
        if (pp_up > 0 && s.pp >= 0 && s.pp < s.maxPP) {
            s.pp = std::min(s.maxPP, s.pp + pp_up);
        }
    }
    // 全零判定（maxPP>0 槽全为 0；maxPP==0 的空槽不参与）
    bool all_zero = !opp.skills.empty();
    for (const Skills& s : opp.skills) {
        if (s.maxPP > 0 && s.pp > 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        ctx->plugin_storage[kDrainFlagBase + owner] = ctx->roundCount + 1;   // 下回合
    }
    return EffectResult::kOk;
}

// ── ④ 追击：旗生效回合内自身用技后 → 消耗对手全部体力 ───────────────────────

EffectResult soulmark_2011_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (watcher_wired(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner
                || !onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const int key = kDrainFlagBase + owner;
            const auto it = wctx->plugin_storage.find(key);
            if (it == wctx->plugin_storage.end()
                || std::any_cast<int>(it->second) != wctx->roundCount) {
                return;   // 非追击回合
            }
            wctx->plugin_storage.erase(it);
            if (wctx->getPet(1 - owner).hp > 0 && g_core
                && g_core->force_hp_to_zero) {
                (void)g_core->force_hp_to_zero(wctx, 1 - owner, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：2011）。四节点全 STAGE 声明（更新器每回合重注册，SOP §一.1）+
// （boss无效）全节点标注（k173 形）。
SOUL_PROGRAM(2011, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE,
                                    &soulmark_2011_arm,
                                    /*once=*/false, /*early=*/true, SoulScope::STAGE,
                                    /*boss_invalid=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START,
                                    &soulmark_2011_round_start,
                                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                    /*boss_invalid=*/true),
                    SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT,
                                    &soulmark_2011_fmr,
                                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                    /*boss_invalid=*/true),
                    SoulMarkNodeRef(State::BATTLE_ROUND_END,
                                    &soulmark_2011_round_end,
                                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                                    /*boss_invalid=*/true)});
