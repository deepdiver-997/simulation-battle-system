// K252 · 爬坡毒+回合类门减伤+提升门增伤族 —— effect 1177（1 宏 1 行）
//                                                            （2026-10-13，夜跑 k-prod-6，worktree ../agents/k252）
//
// 行清单（tools/soul_effects_dump.py 1177，2026-10-13）：
//   1177 row891（泰拉图斯 3595）args[40,10,80,40,40,1]（kind[1,3]，单行）
//     「使用攻击技能40%令对手中毒，每次使用概率增加10%，最高80%；自身处于回合类效果时
//     受到的攻击伤害减少40%，自身处于能力提升状态时造成的攻击伤害增加40%（boss无效）」
//   → db[0]=毒基础%；db[1]=爬坡步；db[2]=毒上限%；db[3]=回合类门减伤%；
//     db[4]=提升门增伤%；db[5]=1 不消费（K2 纪律记档）。tips 为权威文（SOP §六.1）。
//
// 实现口径：
//   - 爬坡毒 = EVENT_SKILL_USED watcher（own + !used_attribute）→ 申报制
//     apply_anomaly_checked 中毒 cur%（F9 审计槽记 cur）→ **每次使用后爬坡**
//     （官方"每次使用概率增加10%"——与 662"未触发才爬"不同源，按行文逐次爬），
//     cur = min(db[2], cur + db[1])，账本持久无重置（tips 无重置子句，记档）；
//   - 回合类门减伤 = REDUCE_PCT + MITIGATE 票（early arm-once）：回调门
//     ctx->has_round_effects(bucket_owner)（battleContext.h 公有查询）→
//     d.final×(100-db[3])/100（只红伤）；
//   - 提升门增伤 = FIRST/SECOND_SKILL_EFFECT 节点（636 形）：自身任一等级槽 > 0
//     （k172 判据）且 roundChoice 为攻击技 → ws.damage_add_pct[owner] += db[4]
//     （1576 钦定通道）；
//   - （boss无效）：K190 口径 boss 旗忽略不置位（记档）。
//
// 动工前置三查（2026-10-13，author k-prod-6 第三轮）：①内核 grep 1177 无魂印行为
//   硬编码；②effect_icon kind=[1,3]、tips 为权威文；③registry 无 1177、soul_lib
//   并集（hub+feat/k226-k251）无 1177——真缺口。
// 先例：K226 662（申报制+审计槽+爬坡，本单改为逐次爬）、K230（红伤门）、
//   K238（has_round_effects 夹具同源查询）、636/1576（SKILL_EFFECT 门 + damage_add_pct
//   钦定通道）、k172（任一等级槽判据）。
// 场景回归：test/scenario/scenario_4020_climb_poison_roundgate_boost_1177.cpp（号段 4020-4029）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMark = 1177;
constexpr int kWiredKeyBase = 991452000;      // +owner：watcher 已挂旗标
constexpr int kPoisonChanceBase = 991453000;  // +owner：毒当前概率（爬坡账本）
constexpr int kPoisonAuditBase = 991454000;   // +owner：本次申报%（审计槽，只读）

bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::CONVERTED;
}

bool has_any_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < 6; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
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

// ── early：爬坡毒 watcher + 回合类门减伤票（arm-once）────────────────────────

EffectResult soulmark_1177_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int base_pct = db_at(row, 0, -1);
    const int step = db_at(row, 1, -1);
    const int cap = db_at(row, 2, -1);
    const int round_gate_pct = db_at(row, 3, -1);
    if (base_pct < 0 || step <= 0 || cap <= 0 || round_gate_pct <= 0) {
        return EffectResult::kOk;   // 无参行不武装（记档）
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(key, 1);

    // 回合类门减伤票
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, round_gate_pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, kMark)) {
                return;
            }
            // 门：自身处于回合类效果（与 break_round_effects 的"有可清回合类"同判定：
            // 技能/魂印桶计数 或 rule_center 名下回合类封属——K238 夹具同源）
            if (!c->has_round_effects(bucket_owner)
                && !c->rule_center_.has_round_type(bucket_owner)) {
                return;
            }
            d.final = d.final * (100 - round_gate_pct) / 100;
        });

    // 爬坡毒 watcher（逐次爬：申报后即爬，落地与否不影响）
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, base_pct, step, cap](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.used_attribute) {
                return;
            }
            if (!onstage_has_soul_mark(wctx, owner, kMark)) {
                return;
            }
            int cur = base_pct;
            const auto it = wctx->plugin_storage.find(kPoisonChanceBase + owner);
            if (it != wctx->plugin_storage.end()) {
                cur = any_int_or(it->second, base_pct);
            }
            wctx->plugin_storage[kPoisonAuditBase + owner] = cur;    // F9 审计槽（申报值）
            if (g_core && g_core->apply_anomaly_checked) {
                (void)g_core->apply_anomaly_checked(
                    wctx, 1 - owner, static_cast<int>(AbnormalStatusId::Poison),
                    random_anomaly_duration(), owner, cur, ChanceSource::SoulMark);
            }
            wctx->plugin_storage[kPoisonChanceBase + owner] =
                cur >= cap ? cap : std::min(cap, cur + step);   // 逐次爬，封顶
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── SKILL_EFFECT：提升门 → 造成的攻击伤害 +db[4]%（1576 通道）───────────────

EffectResult soulmark_1177_skill_effect(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMark)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只增攻击技
    }
    if (!has_any_boost(ctx, owner)) {
        return EffectResult::kOk;   // 门：自身处于能力提升状态（k172 判据）
    }
    const HostRow row = host_row(ctx, args, kMark);
    const int boost_pct = db_at(row, 4, -1);
    if (boost_pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[owner] += boost_pct;
    return EffectResult::kOk;
}

}  // namespace

// 注册（+1 宏：1177）。early=减伤票+爬坡毒 watcher；SKILL_EFFECT=提升门增伤。
SOUL_PROGRAM(1177, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_1177_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT,
                    &soulmark_1177_skill_effect, /*once=*/false, /*early=*/false),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT,
                    &soulmark_1177_skill_effect, /*once=*/false, /*early=*/false)});
