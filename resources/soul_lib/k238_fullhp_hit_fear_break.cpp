// K238 · 满体受击害怕/消除回合类族 —— effect 883（1 宏 1 行）
//                                                            （2026-10-08，夜跑 k-prod-6，worktree ../agents/k238）
//
// 行清单（tools/soul_effects_dump.py 883，2026-10-08）：
//   883 row596（pet 3270）args[1,0]（kind[8,10]，单行）
//     「自身满体力时受到攻击则使对手害怕，未触发则消除对手回合类效果（BOSS无效）」
//   → db[0]/db[1] 不消费（tips 无数值子句，K2 纪律记档）。tips 为权威文（SOP §六.1）。
//
// 实现口径（undying_1067 待触发旗直系同构，文案族"受击触发+未触发断回合"）：
//   - ROUND_START：满体门（self.hp == max）→ 置本回合待触发旗，非满清旗；
//   - 受击触发 = EVENT_TAKE_DAMAGE watcher（arm-once，红伤 target=own、amount>0、
//     K230 同门）：有旗 → 消费 + apply_anomaly_checked 害怕 100%（申报制；落地集
//     SUCCESS/EXTENDED/REPLACED/CONVERTED）——**未落地（免怕/抗性）→ 当场
//     break_round_effects(对手)**（"未触发"读法之一）；
//   - ROUND_END 兜底：旗仍未消费（本回合没人打我）→ break_round_effects(对手)
//     （"未触发"读法之二——两读法并集，K6 1576 同款记档口径）；
//   - 待触发旗放 plugin_storage（side 域；切场边界记档不覆盖——1067 系 soulmark_storage
//     域差异不改变本单行为面）。
//   （BOSS无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-08，author k-prod-6）：①内核 grep 883 无魂印行为硬编码；
//   ②effect_icon kind=[8,10]、tips 为权威文；③registry 无 883、soul_lib 并集
//   （hub+feat/k226-k237）无 883——真缺口。
// 先例：undying_1067（待触发旗+受击触发+ROUND_END 未触发断对手回合类 全形）、
//   K230（TAKE_DAMAGE 红伤门）、miaoshi（落地集）、K226（100% 档确定性）。
// 场景回归：test/scenario/scenario_3820_fullhp_hit_fear_break_883.cpp（号段 3820-3829）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 883;
constexpr int kWiredKeyBase = 991437000;   // +owner：watcher 已挂旗标
constexpr int kPendingBase = 991438000;    // +owner：本回合待触发旗（1/0）

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

// ── ROUND_START：满体门 → 待触发旗 ─────────────────────────────────────────

EffectResult soulmark_883_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const bool full = self.hp > 0 && self.hp == resolve_pet_max_hp(self);
    ctx->plugin_storage[kPendingBase + owner] = full ? 1 : 0;
    return EffectResult::kOk;
}

// ── early：受击触发 watcher（arm-once）──────────────────────────────────────

EffectResult soulmark_883_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || ev.actor == owner || ev.amount <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            const auto pit = wctx->plugin_storage.find(kPendingBase + owner);
            if (pit == wctx->plugin_storage.end() || any_int_or(pit->second, 0) != 1) {
                return;   // 非满体（无旗）
            }
            wctx->plugin_storage.erase(pit);   // 触发即消费
            bool landed = false;
            if (g_core && g_core->apply_anomaly_checked) {
                landed = anomaly_landed(g_core->apply_anomaly_checked(
                    wctx, 1 - owner, static_cast<int>(AbnormalStatusId::Fear),
                    random_anomaly_duration(), owner, 100, ChanceSource::SoulMark));
            }
            // 未触发（免怕/抗性）→ 当场断对手回合类（"未触发"读法一）
            if (!landed && g_core && g_core->break_round_effects) {
                (void)g_core->break_round_effects(wctx, 1 - owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ROUND_END：旗未消费（本回合没挨打）→ 断对手回合类 ───────────────────────

EffectResult soulmark_883_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    const auto pit = ctx->plugin_storage.find(kPendingBase + owner);
    if (pit == ctx->plugin_storage.end()) {
        return EffectResult::kOk;
    }
    const bool pending = any_int_or(pit->second, 0) == 1;
    ctx->plugin_storage.erase(pit);
    if (pending && g_core && g_core->break_round_effects) {
        (void)g_core->break_round_effects(ctx, 1 - owner);   // 读法二：满体且没挨打才断
    }
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：883）。ROUND_START=满体门置旗；early=受击触发 watcher；ROUND_END=兜底断。
SOUL_PROGRAM(883, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_883_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_883_round_start,
                    /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_883_round_end,
                    /*once=*/false, /*early=*/false)});
