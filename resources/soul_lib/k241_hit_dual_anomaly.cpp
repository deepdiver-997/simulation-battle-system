// K241 · 受击双概率异常族 —— effect 804（1 宏 1 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k241）
//
// 行清单（tools/soul_effects_dump.py 804，2026-10-13）：
//   804 row500（银月狼王 3171）args[50,50]（kind[10]，单行）
//     「受到攻击时50%使对手冰封，若不触发则有50%概率令对手麻痹（BOSS无效）」
//   → db[0]=冰封申报%；db[1]=麻痹申报%（级联：仅冰封未落地时尝试）。tips 为权威文。
//
// 实现口径：
//   - 受击窗 = EVENT_TAKE_DAMAGE watcher arm-once（K230 门：target==own + 红伤
//     ATTACK_DAMAGE 时点 + amount>0 + onstage 宿主）；
//   - 级联申报制（内核单掷禁本地预掷）：apply_anomaly_checked 冰封 db[0]% →
//     **未落地**（免疫/抗性/概率未中）→ apply_anomaly_checked 麻痹 db[1]%；
//     落地集 = SUCCESS/EXTENDED/REPLACED/CONVERTED（miaoshi 集）；
//   - 申报值进审计槽（F9 口径）：冰封/麻痹各自记本次申报%，场景无随机梯度断言；
//   - 冰封落地副作用（冻伤+速度-1，abnormal-types.h:387）由引擎自理；
//   - （BOSS无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-13，author k-prod-6 第三轮）：①内核 grep 804 无魂印行为
//   硬编码；②effect_icon kind=[10]、tips 为权威文（dump 原文已核）；③registry 无
//   804、soul_lib 并集（hub+feat/k226-k240）无 804——真缺口。
// 先例：K230（TAKE_DAMAGE 红伤门形）、K239（级联申报+落地集）、miaoshi 4872
//   （落地集）、K226（审计槽+0/100 短路不耗 rand）。
// 场景回归：test/scenario/scenario_3850_hit_dual_anomaly_804.cpp（号段 3850-3859）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 804;
constexpr int kWiredKeyBase = 991441000;      // +owner：watcher 已挂旗标
constexpr int kIceAuditBase = 991442000;      // +owner：冰封申报%（审计槽，只读）
constexpr int kParaAuditBase = 991443000;     // +owner：麻痹申报%（审计槽；未尝试=-1）

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 宿主行参数（"行≠宏"，K226 同款私副本）────────────────────────────────────

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

// ── early：受击双概率异常 watcher ───────────────────────────────────────────

EffectResult soulmark_804_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int ice_pct = db_at(row, 0, -1);
    const int para_pct = db_at(row, 1, -1);
    if (ice_pct < 0 || para_pct < 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, ice_pct, para_pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.amount <= 0) {
                return;
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const int opp = 1 - owner;
            wctx->plugin_storage[kIceAuditBase + owner] = ice_pct;
            bool ice_landed = false;
            if (g_core && g_core->apply_anomaly_checked) {
                ice_landed = anomaly_landed(g_core->apply_anomaly_checked(
                    wctx, opp, static_cast<int>(AbnormalStatusId::Icebound),
                    random_anomaly_duration(), owner, ice_pct, ChanceSource::SoulMark));
            }
            if (ice_landed) {
                return;   // 级联门：冰封触发则不尝试麻痹
            }
            wctx->plugin_storage[kParaAuditBase + owner] = para_pct;
            if (g_core && g_core->apply_anomaly_checked) {
                (void)g_core->apply_anomaly_checked(
                    wctx, opp, static_cast<int>(AbnormalStatusId::Paralysis),
                    random_anomaly_duration(), owner, para_pct, ChanceSource::SoulMark);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：804）。early=受击双概率异常 watcher（事件面不受更新器重注册影响）。
SOUL_PROGRAM(804, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_804_arm,
                    /*once=*/false, /*early=*/true)});
