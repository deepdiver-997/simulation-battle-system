// K365 低血先制+锁血残血族 —— effect 790（1 宏 / 2 行）
//（深渊狱神·哈迪斯 3141；号段 7100-7109，worktree ../agents/k365，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 790，2026-10-24）：
//   row480 3141 args[2,1,2,3]：
//     「回合开始时若自身体力低于对手则当回合所有技能先制+2，同时当回合受到攻击时
//       使对手害怕（害怕效果boss无效）；登场首回合受到致命伤害时残留1点血，同时
//       下回合自身攻击技能先制+2且附加对手最大体力1/3的固定伤害」
//   row611 3141 args[2,1,3,3]：同文，②先制 2→3。
//   → db[0]=①当回合先制+N（两行恒 2）；db[1]=②残留血量（两行恒 1，tips"1点"自证）；
//     db[2]=②下回合先制+N（2/3）；db[3]=②固伤分母（1/3）。⚠️ 行参全在前两位
//     （bind_soulmark_args 丢弃前两位）→ 类型化 a.row 恒空，走 owned_int_args 原序
//     （K266 记档同款）。
// ⚠️ 三查记录（author a22-kprod 2026-10-24）：①内核 grep \b790\b 无魂印行为硬编码；
//    ②effect_info 790 = 技能空间模板「{0}回合内自身所有攻击无视伤害限制效果」（异文，
//      魂印以 effect_icon tips 为权威）；③registry soul 无 790、soul_lib 全库 grep
//      无同 id 注册——真缺口。
// 实现（板行处方；boss 标注单位=子句：①全链 boss_invalid，②照常）：
//   ①「回合开始时低血→当回合先制+2 + 当回合受击害怕」：
//     - BATTLE_ROUND_START 节点（boss_invalid）：onstage 且 self.hp < opp.hp（回合
//       开始时显式锚文，K266 366 无锚文的直读形态不适用）→ 记 soulmark_storage
//       旗标 low_hp_round=roundCount；
//     - FMR 节点（boss_invalid）：旗标==本回合 → ws.preemptive_level += db[0]
//       （pl memset 在 MOVE_RIGHT 时点，SOP §二.2；K266/K218 同款回合级写，"所有
//       技能"无逐技先制通道，记档）；
//     - 受击害怕：arm 时挂 EVENT_TAKE_DAMAGE watcher（永久），回调门
//       low_hp_round==roundCount 且攻击伤害两时点（k319 is_attack_damage_event
//       同款）→ g_core->apply_anomaly(对手, Fear, random_anomaly_duration(), owner)
//       （j_fear_fallback_sixslot 害怕时长先例；已害怕重复施加静默失败，无害）。
//   ②「登场首回合致命伤残留1血→下回合先制+N + 攻击附加对手最大体力1/3固伤」：
//     - enter 节点记 entry_round（K218 同款，early 双触发幂等）；
//     - 锁血：TAKE_DAMAGE watcher（undying 1067 通道）：self.hp==0 且
//       entry_round==roundCount（登场首回合锚）且未消费 → hp=db[1]、记
//       lethal_round=roundCount（每场一次，1067 口径）；
//     - FMR 节点：lethal_round==roundCount-1 → pl += db[2]；
//     - 攻击 rider：BATTLE_FIRST/SECOND_SKILL_EFFECT 双态节点（693 同款）：
//       lethal_round==roundCount-1 且所选为攻击技（roundChoice 面板读法）→
//       deal_pink_damage(对手, resolve_pet_max_hp(对手)/db[3], FIXED, owner)。
// 注册：SOUL_PROGRAM(790) 七节点，SOUL_PROGRAM 总数 +1（只增不改）。
// 场景回归：test/scenario/scenario_7100_lowhp_preempt_lock.cpp（号段 7100-7109）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 790;
constexpr int kPreempt1Fallback = 2;   // db[0] 兜底（两行字面 +2）
constexpr int kLockHpFallback = 1;     // db[1] 兜底（tips"残留1点血"）
constexpr int kPreempt2Fallback = 2;   // db[2] 兜底（row480 字面 +2）
constexpr int kDenomFallback = 3;      // db[3] 兜底（tips"1/3"）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

// ── 跨回合旗标（soulmark_storage 容器，K218Runtime 同款）───────────────────
struct K365Runtime {
    int entry_round = -1;    // 最近一次登场的回合（登场首回合锚）
    int low_hp_round = -1;   // 回合开始时"体力低于对手"成立的回合（①）
    int lethal_round = -1;   // 登场首回合触发锁血的回合（②）
    bool lock_consumed = false;   // 锁血每场一次（undying 1067 口径）
    bool watchers_armed = false;
};

K365Runtime& k365_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K365Runtime{};
    }
    return std::any_cast<K365Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——行参全在前两位，K266 记档）─────
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

// 只认两攻击伤害时点（k319 同款就地复刻——"受到攻击"限攻击系）。
bool is_attack_damage_event(const BattleEvent& ev) {
    return ev.state == static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
        || ev.state == static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE);
}

// ── arm：登场锚 + 双 watcher（锁血② + 受击害怕①；boss 分工在回调门）────────
EffectResult k365_790_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    K365Runtime& rt = k365_runtime(ctx, owner);
    rt.entry_round = ctx->roundCount;   // 每次登场刷新（early 双触发同值幂等）
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (rt.watchers_armed) {
        return EffectResult::kOk;
    }
    rt.watchers_armed = true;

    // ② 锁血：致命伤残留 db[1] 血（undying 1067 通道；登场首回合锚 + 每场一次）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != owner || ev.actor == owner) {
                return;
            }
            K365Runtime& run = k365_runtime(c, owner);
            ElfPet& self = c->getPet(owner);
            if (self.hp != 0 || run.lock_consumed) {
                return;
            }
            if (run.entry_round != c->roundCount) {
                return;   // 仅登场首回合（tips 锚文）
            }
            int slot = c->find_pet_with_soulmark(owner, kMarkId);
            if (slot < 0 || slot > 5) {
                return;
            }
            const auto& db =
                c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
            int lock_hp = kLockHpFallback;
            if (db.size() > 1 && db[1] > 0) {
                lock_hp = db[1];
            }
            run.lock_consumed = true;   // 先耗，幂等
            run.lethal_round = c->roundCount;
            self.hp = lock_hp;          // 残留 1 点体力
        },
        WatcherScope::TEAM);

    // ① 受击害怕：当回合（低血旗标回合）受到攻击 → 对手害怕（boss 无效由
    //   ROUND_START 门自然承接——boss 局旗标永不置位）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount,
        /*duration_rounds=*/0, /*once=*/false,
        [owner](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly) {
                return;
            }
            // ⚠️ ev.actor 可能 -1——范围守卫（k319 同款）。
            if (ev.target != owner || ev.actor < 0 || ev.actor > 1 ||
                ev.actor == owner) {
                return;
            }
            const K365Runtime& run = k365_runtime(c, owner);
            if (run.low_hp_round != c->roundCount) {
                return;   // 非低血旗标回合
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId) ||
                !is_attack_damage_event(ev)) {
                return;
            }
            (void)g_core->apply_anomaly(c, ev.actor, kFear,
                                        random_anomaly_duration(), owner);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── ① 回合开始低血门（"回合开始时"显式锚 → 在此评估，FMR 消费）─────────────
EffectResult k365_790_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const ElfPet& opp = ctx->getPet(ctx->opponent(owner));
    if (self.hp > 0 && opp.hp > 0 && self.hp < opp.hp) {
        k365_runtime(ctx, owner).low_hp_round = ctx->roundCount;
    }
    return EffectResult::kOk;
}

// ── ① FMR：当回合低血 → pl += db[0]（boss_invalid）────────────────────────
EffectResult k365_790_move_right_1(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K365Runtime& rt = k365_runtime(ctx, owner);
    if (rt.low_hp_round != ctx->roundCount) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int preempt = db_at(row, 0, kPreempt1Fallback);
    if (preempt > 0) {
        ctx->ws.preemptive_level[owner] += preempt;
    }
    return EffectResult::kOk;
}

// ── ② FMR：上回合锁血 → pl += db[2]（boss 照常）───────────────────────────
EffectResult k365_790_move_right_2(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K365Runtime& rt = k365_runtime(ctx, owner);
    // ⚠️ 哨兵 -1 会与首回合 roundCount-1=-1 相撞（T7100 实测 pl=4）——先排除未触发。
    if (rt.lethal_round < 0 || rt.lethal_round != ctx->roundCount - 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int preempt = db_at(row, 2, kPreempt2Fallback);
    if (preempt > 0) {
        ctx->ws.preemptive_level[owner] += preempt;
    }
    return EffectResult::kOk;
}

// ── ② 攻击 rider：上回合锁血 → 本次攻击技附加对手最大体力/db[3] 固伤（693 直系）┘
EffectResult k365_790_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1 || !g_core ||
        !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const K365Runtime& rt = k365_runtime(ctx, owner);
    // ⚠️ 同上：哨兵 -1 与首回合 roundCount-1 相撞防护。
    if (rt.lethal_round < 0 || rt.lethal_round != ctx->roundCount - 1) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // "自身攻击技能"——属性技不附
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 3, kDenomFallback);
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& opp = ctx->getPet(ctx->opponent(owner));
    const int fixed = resolve_pet_max_hp(opp) / denom;
    if (fixed > 0) {
        (void)g_core->deal_pink_damage(ctx, ctx->opponent(owner), fixed,
                                       DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（790 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k365_790_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_ROUND_START, &k365_790_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k365_790_move_right_1,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k365_790_move_right_2,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k365_790_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k365_790_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
