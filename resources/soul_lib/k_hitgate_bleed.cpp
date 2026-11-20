// K376 受伤阈值门+概率流血族 —— effect 340（1 宏 1 行）
//
// 行清单（tools/soul_effects_dump.py 340，2026-10-24）：
//   340 row118（2701 太古邪魔「霸」）args[250,30,16]（零兄弟行，kind=[]）：
//     「自身受到的伤害不足250，则战斗阶段结束时对手有30%概率流血」
//     → db[0]=阈值250、db[1]=概率30%、db[2]=流血码16（三参行文互锚自洽；
//       effect_info 无 340 行——无模板异文问题）。
//
// 落法（板行处方）：
//   ① DETECT 相位读快照（K189 1016/K203 344 直系；SOP §三.1【K1】读快照 watcher
//      不受 AFTER_ACTION 时点坑—— DETECT 相位在管线内同步可读 resolvedDamage）：
//      受击方桶 d.isRed、0 < d.final < db[0] → 置回合键旗（"不足250"严格小于；
//      final<=0 早退 = 被免/归零的 0 伤不算"受到的伤害"，K203 形纪律记档）。
//   ② BATTLE_ROUND_END（"战斗阶段结束时"字面，chaos_sorensen 1011 同款时点桶）：
//      旗在 → 清旗 + apply_anomaly_checked(对手, db[2]=流血16, db[1]=30%,
//      SoulMark)（概率申报通道单掷，不预掷——K191 双掷勘误）。
//   ③ 流血(16) 属 K2 时代『非 0-9 段先跳』旧口径之外：引擎现状
//      is_valid_abnormal_status_id 全 0..43 通过 + Bleed tick 已建模
//      （abnormal-types.h:361「每回合对手行动开始时扣除 80 点体力」）+
//      Blind(20) 已被 K369/miaoshi_4872 实证可施加——同路线解锁（工单行备案）。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B）；tips 无 boss 尾注 →
//   节点不带 boss_invalid（任何对局照常）。审计槽 990376800 段（F9 782 技巧）。
//
// 三查记录（2026-10-24，worktree ../agents/k376）：
//   ①内核 grep 340 无魂印行为硬编码；②effect_icon kind=[]、tips 为权威文
//   （dump 原文已核，见上；effect_info 无 340）；③registry soul 集合无 340、
//   soul_lib 全库 grep 无 SOUL_PROGRAM(340)——真缺口。
// 先例：k189 1016（DETECT 阈值分叉+旗标形）、k203 344（DETECT 读快照）、
//   chaos_sorensen 1011（BATTLE_ROUND_END=战斗阶段结束时）、k366 1210
//   （apply_anomaly_checked 申报+审计槽同款）。
// 场景回归：test/scenario/scenario_7102_k376_hitgate_bleed.cpp（号段 7102）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 340;
constexpr int kWiredBase = 990376000;      // 接线去重：+宏*10+owner（grep 无撞）
constexpr int kPendingFlagBase = 990376500;  // 受低伤待结算旗：+owner
constexpr int kAuditPctBase = 990376800;   // 审计槽：+owner*2=申报概率；+1=结果

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）────────
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

bool watcher_wired(BattleContext* ctx, int owner) {
    const int key = kWiredBase + kMarkId * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── arm（enter/early）：DETECT 阈值门（受低伤置旗）───────────────────────────
EffectResult k340_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int threshold = db_at(row, 0, 0);
    if (threshold <= 0) {
        return EffectResult::kOk;   // 无参不挂（K1 纪律）
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, threshold](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (bucket_owner != owner || d.defenderId != bucket_owner || !d.isRed
                || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            if (d.final < threshold) {
                c->plugin_storage[kPendingFlagBase + owner] = 1;   // "不足250"
            }
        });
    return EffectResult::kOk;
}

// ── ROUND_END：读旗清旗 → 对手 30% 概率流血 ─────────────────────────────────
EffectResult k340_round_end_bleed(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kPendingFlagBase + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
        return EffectResult::kOk;   // 本回合未受低伤
    }
    ctx->plugin_storage.erase(it);   // 一次性消费
    const HostRow row = host_row(ctx, a, kMarkId);
    const int bleed_code = db_at(row, 2, 0);
    const int bleed_pct = db_at(row, 1, 0);
    if (bleed_code <= 0 || bleed_pct <= 0) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult res = g_core->apply_anomaly_checked(
        ctx, 1 - owner, bleed_code, random_anomaly_duration(), owner, bleed_pct,
        ChanceSource::SoulMark);
    // 审计槽（F9 782 技巧）：场景对概率做无随机精确计数
    ctx->plugin_storage[kAuditPctBase + owner * 2] = bleed_pct;
    ctx->plugin_storage[kAuditPctBase + owner * 2 + 1] = static_cast<int>(res);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（340 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k340_arm,
                             /*once=*/false, /*early=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_END, &k340_round_end_bleed,
                             /*once=*/false, /*early=*/false)});
