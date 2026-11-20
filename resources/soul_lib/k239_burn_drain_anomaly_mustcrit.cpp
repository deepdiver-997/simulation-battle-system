// K239 · 必发烧伤+吸伤回血+异常门必暴族 —— effect 1899（1 宏 1 行）
//                                                            （2026-10-12，夜跑 k-prod-6，worktree ../agents/k239）
//
// 行清单（tools/soul_effects_dump.py 1899，2026-10-12）：
//   1899 row1535（灼日之蚀 4313）args[100,100,1]（kind[1,5]，单行）
//     「自身攻击技能命中后100%令对手烧伤，若对手已处于烧伤则造成攻击伤害的100%恢复
//     自身体力；战斗阶段结束时若对手处于异常状态则自身下回合攻击必定打出致命一击
//     （boss无效）」
//   → db[0]=烧伤申报%（100）；db[1]=吸伤回血比%（100）；db[2]=1 不消费（K2 纪律记档）。
//     tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   - 攻击命中窗 = EVENT_TAKE_DAMAGE watcher arm-once（K1 766 门：ev.actor==own +
//     ev.state ∈ FIRST/SECOND_ATTACK_DAMAGE + ws.raw_attack_damage>0——"命中后"=
//     实际造成伤害）：
//     ① 对手已处于烧伤（g_core->has_active_anomaly 单 id 查——插件不可链
//     has_active_abnormal_status，author-4 链接错记档）→ heal_amount(raw×db[1]%)
//     （"攻击伤害的100%"= 裸伤台账读数，K1 766 同源）；② apply_anomaly_checked
//     烧伤 db[0]%（100 档确定性短路不耗 rand，K226 口径）——先判已烧再施加
//     （已烧时施加走 DURATION_EXTENDED 刷新，落地集照记）；
//   - 异常门必暴 = ROUND_END：has_any_anomaly(对手)（k227 全池扫）→ pending =
//     roundCount+1（K227 "下回合"账本形）；FMR 节点：pending == roundCount →
//     ws.must_crit[owner] = true（227 卢恩斯/encarlos 必暴套路，must_crit 每回合
//     reset → FMR 写入覆盖整回合）；玩家侧对手异常常驻时逐回合续写。
//   （boss无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-12，author k-prod-6 第二轮）：①内核 grep 1899 无魂印行为
//   硬编码；②effect_icon kind=[1,5]、tips 为权威文（dump 原文已核）；③registry 无
//   1899、soul_lib 并集（hub+feat/k226-k240 在途分支）无 1899——真缺口。
// 先例：K1 766（TAKE_DAMAGE 攻击窗+raw_attack_damage 读数）、K230（红伤门形）、
//   k227 2011（has_any_anomaly 全池扫 + "下回合"pending 账本 + FMR 消费）、
//   chaos_mozun/encarlos（ws.must_crit 必暴写）、K226（100% 档申报制）。
// 场景回归：test/scenario/scenario_3830_burn_drain_anomaly_mustcrit_1899.cpp（号段 3830-3839）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1899;
constexpr int kWiredKeyBase = 991439000;      // +owner：watcher 已挂旗标
constexpr int kMustCritRoundBase = 991440000; // +owner：必暴回合号（下回合 pending）

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

bool has_anomaly(BattleContext* ctx, int side, int id) {
    return g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, side, id);
}

// "处于异常状态" = 任一官方异常槽活跃（k227 全池扫同款）。
bool has_any_anomaly(BattleContext* ctx, int side) {
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (has_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
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

// ── early：攻击命中窗 watcher（烧伤 + 已烧吸伤回血）─────────────────────────

EffectResult soulmark_1899_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int burn_pct = db_at(row, 0, -1);
    const int heal_pct = db_at(row, 1, -1);
    if (burn_pct < 0) {
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
        [owner, burn_pct, heal_pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.amount <= 0) {
                return;   // 只认"自己攻击对手"造成的伤害（K1 766 门）
            }
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const int opp = 1 - owner;
            const int raw = wctx->ws.raw_attack_damage[owner];
            if (raw <= 0) {
                return;
            }
            // 已处于烧伤 → 攻击伤害的 db[1]% 恢复自身（先判已烧再施加）
            if (heal_pct > 0 && has_anomaly(wctx, opp,
                                            static_cast<int>(AbnormalStatusId::Burn))
                && g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(wctx, owner, raw * heal_pct / 100);
            }
            if (burn_pct > 0 && g_core && g_core->apply_anomaly_checked) {
                (void)g_core->apply_anomaly_checked(
                    wctx, opp, static_cast<int>(AbnormalStatusId::Burn),
                    random_anomaly_duration(), owner, burn_pct, ChanceSource::SoulMark);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── FMR：消费"下回合必暴"pending → ws.must_crit ────────────────────────────

EffectResult soulmark_1899_fmr(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kMustCritRoundBase + owner);
    if (it == ctx->plugin_storage.end()
        || any_int_or(it->second, -1) != ctx->roundCount) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(it);
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// ── ROUND_END：对手处于异常 → 下回合必暴 pending ────────────────────────────

EffectResult soulmark_1899_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    if (!has_any_anomaly(ctx, 1 - owner)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[kMustCritRoundBase + owner] = ctx->roundCount + 1;
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1899）。early=攻击命中窗 watcher；FMR=必暴消费；ROUND_END=异常门置 pending。
SOUL_PROGRAM(1899, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1899_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_1899_fmr,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_1899_round_end,
                    /*once=*/false, /*early=*/false, SoulScope::STAGE,
                    /*boss_invalid=*/true)});
