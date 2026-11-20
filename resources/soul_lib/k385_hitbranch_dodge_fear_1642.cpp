#include <utils/sbs_rand.h>
// K385 受击分叉随机强化+条件闪避害怕族 —— effect 1642（1 宏 / 2 行）
//（环·木心 row1302 args[3,1,3,1,70,70,1] 本体 + row2136 args[6,1,6,1,100,100,1]
//  伙伴行；号段 7360-7369，worktree ../agents/k385，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1642，2026-10-24）：
//   row1302 本体：「战斗阶段结束时，若当回合自身受到了攻击伤害则使自身随机3项能力等级
//     +1，若当回合自身未受到攻击伤害则令对手随机3项能力等级-1；回合开始时，若自身处于
//     能力提升状态则当回合70%闪避对手攻击，若对手处于能力下降状态则当回合攻击命中后
//     70%令对手害怕」（boss无效）
//   row2136 伙伴行：同文 随机3项→全能力（db0/db2=6）、70%→100%（db4/db5）。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a79-jprod 动工同日复核）：
//   ①内核 grep \b1642\b 无魂印行为硬编码；②effect_info 1642 异文（消强+致命印记
//   模板——tips 权威）；③registry soul 无 1642、soul_lib 全库 grep 无同 id 注册
//   ——真缺口。
//
// 行参解码（两行同构互锚）：
//   db[0]=①受击分支自身项数（3=随机3项/6=全）、db[1]=旗、db[2]=①未受击分支对手
//   项数（3/6）、db[3]=旗、db[4]=②闪避%（70/100）、db[5]=③害怕%（70/100）、
//   db[6]=1 旗（不消费——K1 纪律）。
//
// 实现（板行处方）：
//   ① ROUND_END：受击账本（DETECT 票：defender==own && isRed && final>0 → 记
//     roundCount——"受到**攻击**伤害"只认红伤，K229 口径）== 当回合 → 自身随机
//     db[0] 项 stat_change +1（db0==6 → 全 6 槽；增益通道不查免弱，K282 645 同款，
//     原语内钳 ±6）；未受击 → 对手随机 db[2] 项 stat_drop -1（弱化通道查免弱，
//     db2==6 → 全 6 槽；k893/K268 去重选槽）。项数断言 = level_sum 增量（槽位随机
//     不可断，和可断）。
//   ② ROUND_START：自身任一 ability_levels > 0（"能力提升状态"持续态，K253 判据
//     取正）→ ws.dodge_rate[own] = db[4]/100（k_hit_dodge_aura 同位；ws 每回合
//     reset → 条件不满足自然回落 0，无残留）。
//   ③ DETECT（own 红伤同步查询——K1 裁定条件查询走管线相位）：对手任一
//     ability_levels < 0 → apply_anomaly_checked(对手, 害怕6, duration=
//     random_anomaly_duration()（inline 恒 2，K253 口径），own, db[5], SoulMark)
//     （概率申报版不预掷，K191 教训；100% 短路不耗 rand）。
// 注册：SOUL_PROGRAM(1642) 三节点（arm + ROUND_START + ROUND_END），SOUL_PROGRAM
// 总数 +1（只增不改）；tips 尾注（boss无效）→ 全节点 boss_invalid=true。
// 场景回归：test/scenario/scenario_7360_hitbranch_dodge_fear_1642.cpp（7360-7369）。

#include "common.h"

#include <functional>

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1642;
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);

// ── 账本键（plugin_storage，+own 分位）────────────────────────────────────────
constexpr int kWiredKeyBase = 990385000;   // +owner：票已武装旗标
constexpr int kHitRoundKeyBase = 990385100;  // +owner：最近受红伤的回合数（0=无）

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K382/K384 同款）─────────────
struct HostRow {
    const std::vector<int>* db = nullptr;
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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int read_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 持续态门：任一能力槽为正（提升）/负（下降）——K253 同款判据（ability_levels
// 在 stat_change/stat_drop 落点即时写入，回合边界不清零）。
bool any_level_sign(BattleContext* ctx, int side, bool positive) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (positive ? (ctx->ability_levels[side][i] > 0)
                     : (ctx->ability_levels[side][i] < 0)) {
            return true;
        }
    }
    return false;
}

// 随机 count 项去重选槽 → fn(slot)（k893/K268 去重循环同款）。
void for_random_slots(int count, const std::function<void(int)>& fn) {
    bool picked[6] = {};
    int done = 0;
    while (done < count && done < 6) {
        const int slot = sbs_rand() % 6;
        if (picked[slot]) {
            continue;
        }
        picked[slot] = true;
        fn(slot);
        ++done;
    }
}

// ── arm：武装受击账本票 + 命中害怕票（一次性）─────────────────────────────────
EffectResult k385_1642_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（K226/K382/K384 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    // 受击账本哨兵 -1（⚠️ K363 同款防撞：缺省 0 与首回合 roundCount=0 相撞 →
    //   R1 空窗误判"已受击"——武装时显式写 -1，roundCount ≥ 0 永不命中哨兵）。
    ctx->plugin_storage[kHitRoundKeyBase + owner] = -1;
    const HostRow row = host_row(ctx, a, kMarkId);
    const int fear_pct = db_at(row, 5, 70);

    // 票A：受击账本（①的"当回合受到攻击伤害"——红伤 only）
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            c->plugin_storage[kHitRoundKeyBase + owner] = c->roundCount;
        });

    // 票B：own 攻击命中 + 对手降态 → 概率害怕（DETECT 相位同步查询——K1 裁定）
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, fear_pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core ||
                !g_core->apply_anomaly_checked) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = 1 - owner;
            if (!any_level_sign(c, opp, /*positive=*/false)) {
                return;   // 对手无降态 → 门关
            }
            (void)g_core->apply_anomaly_checked(c, opp, kFearId,
                                                random_anomaly_duration(), owner,
                                                fear_pct, ChanceSource::SoulMark);
        });
    return EffectResult::kOk;
}

// ── ROUND_START：② 提升态 → 当回合闪避窗 ─────────────────────────────────────
EffectResult k385_1642_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (!any_level_sign(ctx, owner, /*positive=*/true)) {
        return EffectResult::kOk;   // 无提升态 → ws 已 reset，闪避率自然为 0
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int dodge_pct = db_at(row, 4, 70);
    ctx->ws.dodge_rate[owner] = dodge_pct / 100.0f;
    return EffectResult::kOk;
}

// ── ROUND_END：① 受击分叉 随机强化 / 随机弱化 ────────────────────────────────
EffectResult k385_1642_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int self_count = db_at(row, 0, 3);
    const int foe_count = db_at(row, 2, 3);
    const int opp = 1 - owner;
    const bool hit_this_round =
        read_int(ctx, kHitRoundKeyBase + owner) == ctx->roundCount;
    if (hit_this_round) {
        if (self_count >= 6) {
            for (int i = 0; i < 6; ++i) {
                (void)g_core->stat_change(ctx, owner, i, 1);
            }
        } else {
            for_random_slots(self_count, [&](int slot) {
                (void)g_core->stat_change(ctx, owner, slot, 1);
            });
        }
    } else {
        if (foe_count >= 6) {
            for (int i = 0; i < 6; ++i) {
                (void)g_core->stat_drop(ctx, opp, i, 1, owner);
            }
        } else {
            for_random_slots(foe_count, [&](int slot) {
                (void)g_core->stat_drop(ctx, opp, slot, 1, owner);
            });
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1642 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k385_1642_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_START, &k385_1642_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k385_1642_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
