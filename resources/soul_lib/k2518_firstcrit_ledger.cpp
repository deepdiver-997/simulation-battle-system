// K381 首攻必爆+消强消回合+爆次对账族 —— effect 2518（1 宏 2 行）
//
// 行清单（tools/soul_effects_dump.py 2518，2026-10-24）：
//   2518 row2095（4908）args[1,0]本体：
//     「每次在场自身的首次使用的攻击技能必定打出致命一击，在打出之前每回合战斗阶段
//      结束时消除对手能力提升状态，消除成功则消除对手回合类效果|战斗阶段结束时，
//      若本次在场自身打出致命一击的次数高于对手则将对手所处的异常状态转化为1回合
//      焚烬，若低于对手则恢复双方最大体力差值的体力，若等于对手则下回合所有技能
//      先制+1|（boss无效）」
//   2518 row2107（黄金圣徒伙伴效果行）args[1,1]：焚烬 2 回合、先制+3，其余同构互锚。
//
// 行参解码（两行互锚）：db[0]=旗（不消费，记档）；db[1]=伙伴位→**焚烬回合基数**
//   （0→覆盖 1 整轮、1→2 整轮；duration 参=db[1]+2，引擎口径见 settle 注）兼
//   **先制档位选择子**（0→+1、1→+3——先制级数无线性映射，按 db[1] 分支由行文承载，
//   动工口径记档）。行≠模板：effect_info
//   2518 异文（"PP 值被清除时 70% 保留"），tips 权威。
//
// 落法（板行处方逐子句）：
//   ① 首攻必爆：ENTER 记 per-epoch 运行时（K218 entry 形；pipeline_valid_id 换人
//      重置——"每次在场"）+ FMR 节点首攻未爆前置 ws.must_crit[owner]=true（暴击
//      判定只对攻击技跑，属性技置位无副作用）；DETECT 攻方票见 isCrit →
//      first_crit_done=true（⚠️ 工程口径：首攻以**首次命中爆击**为准——miss 刀
//      不进 DETECT，记档）。
//   ② 打出前消强消回合：ROUND_END 且 !first_crit_done → clear_stat_boosts(对手)
//      返回数>0（成功）→ break_round_effects(对手)（K309/K177 串联形）。
//   ③ 爆次对账（ROUND_END，累计口径——"本次在场"计数跨回合不清，逐回合结算，
//      记档）：DETECT 票同时记对手命中我方的 isCrit（1v1 全部攻击以我为目标）；
//      my>opp → apply_anomaly(对手, 焚烬 Incinerate=22, db[1]+2 回合参=覆盖 db[1]+1
//      整轮)（确定性档，
//      走非概率槽；"转化"=控场类 REPLACED_EXISTING 由原语内建，非控场并存为引擎
//      口径记档）；my<opp → heal_amount(自身, |max_self-max_opp|)；my==opp →
//      置下回合先制待消费旗。
//   ④ FMR：旗在 → preemptive_level[owner] += (db[1]==0 ? 1 : 3)，清旗。
//   boss 标注：tips 尾注挂在第二分句 → ③④节点置位；①②（首分句）不置位。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B）。运行时存
//   soulmark_storage（K218 Runtime 形，int epoch + any 结构体）。
//
// 三查记录（2026-10-24，worktree ../agents/k381）：
//   ①内核 grep 2518 无魂印行为硬编码；②effect_icon kind=[8,1,5]、tips 为权威文
//   （dump 原文已核，见上）；③registry soul 集合无 2518、soul_lib 全库 grep 无
//   SOUL_PROGRAM(2518)——真缺口。
// 先例：k218 277（ENTER/FMR 先制形）、k309 802（消强→消回合串联）、k203 344
//   （DETECT isCrit 读快照）、k189 1016（FMR 旗标消费）、k366 1210（审计/原语口径）。
// 场景回归：test/scenario/scenario_7280_firstcrit_ledger.cpp（号段 7280-7289）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 2518;
constexpr int kEpochSlot = kMarkId * 100 + 9;      // soulmark_storage epoch 槽
constexpr int kCritTicketSlot = kMarkId * 100 + 8; // DETECT 票接线 epoch 槽
constexpr int kPreemptFlagBase = 990381500;        // 下回合先制待消费旗：+owner

struct Runtime {
    int epoch = -1;
    bool first_crit_done = false;
    int my_crits = 0;
    int opp_crits = 0;
};

Runtime& runtime_of(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = Runtime{};
    }
    return std::any_cast<Runtime&>(storage[kMarkId]);
}

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

// ── ENTER：per-epoch 运行时重置（"每次在场"）+ 挂 DETECT 爆次票 ─────────────
EffectResult k2518_enter(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->getPet(owner);
    // 运行时：epoch 换人（切换/再上场）即重置计数（K218 entry 语义）
    Runtime& rt = runtime_of(ctx, owner);
    if (rt.epoch != ctx->pipeline_valid_id[owner]) {
        rt = Runtime{};
        rt.epoch = ctx->pipeline_valid_id[owner];
    }
    // DETECT 票：每 epoch 一次（K180 幂等守卫形）
    auto it = pet.soulmark_storage.find(kCritTicketSlot);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(kCritTicketSlot, int{-1}).first;
    }
    if (int* epoch = std::any_cast<int>(&it->second)) {
        if (*epoch == ctx->pipeline_valid_id[owner]) {
            return EffectResult::kOk;
        }
        *epoch = ctx->pipeline_valid_id[owner];
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (bucket_owner != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            Runtime& rt = runtime_of(c, owner);
            if (rt.epoch != c->pipeline_valid_id[owner]) {
                return;   // 旧 epoch 票残余（理论不达，双保险）
            }
            if (d.attackerId == owner && d.isCrit) {
                rt.my_crits++;
                rt.first_crit_done = true;   // 首次命中爆击（工程口径见头注）
            } else if (d.attackerId == 1 - owner && d.isCrit) {
                rt.opp_crits++;
            }
        });
    return EffectResult::kOk;
}

// ── FMR（首分句）：首攻未爆 → 本回合必爆旗 ─────────────────────────────────
EffectResult k2518_fmr_must_crit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    Runtime& rt = runtime_of(ctx, owner);
    if (rt.first_crit_done) {
        return EffectResult::kOk;   // 首攻已爆过
    }
    ctx->ws.must_crit[owner] = true;   // 暴击判定只对攻击技跑（属性技置位无副作用）
    return EffectResult::kOk;
}

// ── ROUND_END（首分句）：打出前消强 → 成功则消回合类 ────────────────────────
EffectResult k2518_round_end_purge(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    Runtime& rt = runtime_of(ctx, owner);
    if (rt.first_crit_done) {
        return EffectResult::kOk;   // "在打出之前"——首爆后不再消强
    }
    if (!g_core->clear_stat_boosts || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int cleared = g_core->clear_stat_boosts(ctx, 1 - owner);
    if (cleared > 0) {
        (void)g_core->break_round_effects(ctx, 1 - owner);   // 消除成功 → 消回合类
    }
    return EffectResult::kOk;
}

// ── ROUND_END（次分句，boss_invalid）：爆次对账三岔 ─────────────────────────
EffectResult k2518_round_end_settle(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    Runtime& rt = runtime_of(ctx, owner);
    if (rt.my_crits > rt.opp_crits) {
        // 转化为 N 回合焚烬（确定性档走非概率槽；回合数 db[1]+1）
        if (!g_core->apply_anomaly) {
            return EffectResult::kOk;
        }
        const HostRow row = ctx ? host_row(ctx, a, kMarkId) : HostRow{};
        // ⚠️ 回合数换算（a24-kprod 实测定版 2026-10-24）：引擎 end_round =
        // roundCount + duration（battle_mutate.cpp:92），活跃判据 roundCount <
        // end_round；round N 全程 roundCount=N-1（advanceRound 在 ROUND_COMPLETION）
        // → ROUND_END 桶施加时 rc=N_cur-1，"覆盖后续 N 个整轮"（r(N_cur+1) 起
        // 活跃）→ end=N_cur-1+duration ≥ N_cur+N-1+1 → duration=N+1=db+2。
        // 实测：+1 即刻过期（end=1 vs rc=1）、+3 打穿 r3 断言（前手两改记档作废）。
        // 另：结算每回合重放（"本次在场"累计口径），对手对爆拉平后不再重刷。
        const int burn_rounds = db_at(row, 1, 0) + 2;
        (void)g_core->apply_anomaly(ctx, 1 - owner,
                                    static_cast<int>(AbnormalStatusId::Incinerate),
                                    burn_rounds, owner);
    } else if (rt.my_crits < rt.opp_crits) {
        // 恢复双方最大体力差值的体力（自身侧）
        if (!g_core->heal_amount || ctx->getPet(owner).hp <= 0) {
            return EffectResult::kOk;
        }
        const int max_self = resolve_pet_max_hp(ctx->getPet(owner));
        const int max_opp = resolve_pet_max_hp(ctx->getPet(1 - owner));
        const int diff = max_self > max_opp ? max_self - max_opp
                                            : max_opp - max_self;
        if (diff > 0) {
            (void)g_core->heal_amount(ctx, owner, diff);
        }
    } else {
        // 等于 → 下回合所有技能先制+N（FMR 消费）
        ctx->plugin_storage[kPreemptFlagBase + owner] = 1;
    }
    return EffectResult::kOk;
}

// ── FMR（次分句，boss_invalid）：先制待消费旗 ───────────────────────────────
EffectResult k2518_fmr_preempt(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const auto it = ctx->plugin_storage.find(kPreemptFlagBase + owner);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second, 0) != 1) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage.erase(it);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int tier = db_at(row, 1, 0);
    ctx->ws.preemptive_level[owner] += (tier == 0 ? 1 : 3);   // memset 后写（K218 形）
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（2518 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k2518_enter,
                             /*once=*/false, /*early=*/true, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                       SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT,
                             &k2518_fmr_must_crit,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                       SOUL_NODE(State::BATTLE_ROUND_END, &k2518_round_end_purge,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/false),
                       SOUL_NODE(State::BATTLE_ROUND_END, &k2518_round_end_settle,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT,
                             &k2518_fmr_preempt,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true)});
