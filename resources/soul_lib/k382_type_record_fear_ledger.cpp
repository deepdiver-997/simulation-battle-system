// K382 类型记录+害怕对账+命中吸取族 —— effect 2501（1 宏 / 2 行）
//（安瑟伦（黄金圣徒）4894 row2081 本体 + row2104 伙伴行；号段 7290-7299，
//  worktree ../agents/k382，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 2501，2026-10-24）：
//   row2081 args[100,400,40,20,1] kind[10,14]：
//     「战斗阶段结束时记录对手当回合所使用技能的类型，若该类型与上次记录不同则
//       100%令对手害怕3回合，若该类型与上次记录相同则对手该类型技能下回合先制-1
//       自身攻击技能命中后，若对手处于害怕状态则令对手害怕回合数-1并附加200点
//       固定伤害，每生效1次增加100点，最多400点
//       自身属性技能命中后，若对手处于害怕状态则令对手害怕回合数+1并吸取对手
//       最大体力的40%，每生效1次减少10%，最低20%」（boss无效）
//   row2104 args[100,500,50,30,1]（黄金圣徒伙伴效果）：同文 400→500、40→50、20→30。
//
// ⚠️ 三查记录（author 立单 2026-10-24 / a76-jprod 动工同日复核）：
//   ①内核 grep \b2501\b 无魂印行为硬编码；②effect_info 2501 异文（技能无效重结算
//   ——tips 权威）；③registry soul 无 2501、soul_lib 全库 grep 无同 id 注册——真缺口。
//
// 行参解码（两行同构互锚；200/步进100/步进10%/害怕3回合 无参位——取 tips 字面常量，
//   K265 "解析不了宁可不用"纪律）：
//   db[0]=100（害怕概率位，字面自证，与申报通道同值——不重复消费，记档）
//   db[1]=②固伤上限（400/500）、db[2]=③吸取起始%（40/50）、db[3]=③吸取下限%（20/30）
//   db[4]=1 旗（不消费——K1 纪律）
//
// 实现（板行处方）：
//   ① ROUND_END（战斗阶段结束时）：对手当回合有用技（executed_this_round 对手侧）
//     → 类型 = ws.skill_type_view[opp]；有上次记录且相同 → 置"下回合对手先制-1"戳；
//     不同 → apply_anomaly_checked(对手, 害怕6, duration=3 固定, owner, 100, SoulMark)
//     （100% 短路不耗 rand）；更新 last_type。对手没用技（NONE/被控）→ 不记录不判定。
//   ①' 下回合对手先制-1：FMR 桶读戳（== roundCount-1，K363/K367 同款）→
//     ws.preemptive_level[opp] -= 1（"该类型技能"无逐技先制通道——回合级落地，
//     K363 定例记档）；一次性消费后清戳。
//   ② 攻击命中对账（DETECT 相位伤害效果，k189/K372 同位）：own 红伤 final>0 且
//     对手害怕 → 害怕剩余回合 -1（get_abnormal_status_end_round 读 →
//     set_anomaly_rounds 写；钳 ≥0）+ fixed_damage(对手, bonus)；
//     bonus = min(200 + 100×已生效次数, db[1])（首次 200，封顶 db[1]）。
//   ③ 属性命中对账（FIRST/SECOND_ON_SKILL_HIT 桶——属性技无伤害管线进不了
//     DETECT，G5 常识）：当次用技为属性技（roundChoice 读法）且对手害怕 →
//     害怕剩余回合 +1 + 吸取对手 max×pct（deal_pink_damage FIXED + heal_amount
//     等量，不越过存量——K302/K338 拆原语）；pct = max(db[3], db[2] − 10×已生效次数)。
//   账本：soulmark_storage K382Runtime（last_type/has_last、same_round 戳、
//   atk_procs/attr_procs 梯计数）；DETECT lambda 经 getPet().soulmark_storage 同容器。
// 注册：SOUL_PROGRAM(2501) 五节点（arm + ROUND_END + FMR 戳消费 +
// FIRST/SECOND_ON_SKILL_HIT），SOUL_PROGRAM 总数 +1（只增不改）；tips 尾注（boss无效）→ 全节点 boss_invalid=true。
// 场景回归：test/scenario/scenario_7290_type_record_fear_ledger.cpp（7290-7299）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 2501;
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kFearDuration = 3;      // ① 害怕固定 3 回合（tips 字面）
constexpr int kBonusBase = 200;       // ② 固伤基数（tips 字面）
constexpr int kBonusStep = 100;       // ② 每生效 +100（tips 字面）
constexpr int kDrainStepPct = 10;     // ③ 每生效 -10%（tips 字面）

// ── 账本（soulmark_storage 容器，K218/K363 同款）─────────────────────────────
struct K382Runtime {
    int last_type = -1;      // 上次记录的对手用技类型（-1=未记录）
    int same_round = -1;     // "同类型"戳（ROUND_END 置位，下回合 FMR 消费）
    int atk_procs = 0;       // ② 已生效次数
    int attr_procs = 0;      // ③ 已生效次数
};

K382Runtime& k382_runtime(BattleContext* ctx, int owner) {
    auto& storage = ctx->getPet(owner).soulmark_storage;
    if (!storage.count(kMarkId)) {
        storage[kMarkId] = K382Runtime{};
    }
    return std::any_cast<K382Runtime&>(storage[kMarkId]);
}

// ── 宿主行参数（db_at 原序读 owned_int_args——K279/K363 同款）──────────────────
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

// ⚠️ 插件通道：has_active_abnormal_status / get_abnormal_status_end_round 是
// BattleContext 非 inline 成员，插件不可直调（2026-10-24 接管构建链接实证）——
// 一律走 g_core->has_active_anomaly / get_abnormal_status_end_round。
bool foe_feared(BattleContext* ctx, int opp) {
    return g_core && g_core->has_active_anomaly &&
           g_core->has_active_anomaly(ctx, opp, kFearId);
}

int fear_remaining(BattleContext* ctx, int opp) {
    if (!g_core || !g_core->get_abnormal_status_end_round) {
        return 0;
    }
    const int end_round = g_core->get_abnormal_status_end_round(ctx, opp, kFearId);
    return end_round > ctx->roundCount ? end_round - ctx->roundCount : 0;
}

// 害怕剩余回合 ±delta（set_anomaly_rounds 只刷新已存在的异常）。
void shift_fear_rounds(BattleContext* ctx, int opp, int delta) {
    if (!g_core || !g_core->set_anomaly_rounds || !foe_feared(ctx, opp)) {
        return;
    }
    const int remaining = fear_remaining(ctx, opp);
    int next = remaining + delta;
    if (next < 0) {
        next = 0;
    }
    (void)g_core->set_anomaly_rounds(ctx, opp, kFearId, next);
}

// 对手本回合是否用过技（判据统一 §四点五 私副本——skill_use_seq 增量）。
bool foe_executed_this_round(const BattleContext* ctx, int opp) {
    if (!ctx || opp < 0 || opp > 1) {
        return false;
    }
    return ctx->skill_use_seq[opp] > ctx->ws.skill_use_seq_round_start[opp];
}

// 所选技是不是攻击技（roundChoice 读法，k300 同款）。
bool used_attack_skill(BattleContext* ctx, int side) {
    if (!ctx || ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return false;
    }
    const int idx = ctx->roundChoice[side][1];
    if (idx < 0 || idx >= 5) {
        return false;
    }
    return ctx->getPet(side).skills[idx].type != SkillType::Attribute;
}

// ── ① ROUND_END：类型记录 + 同/异分叉 ────────────────────────────────────────
EffectResult k382_2501_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    if (ctx->getPet(owner).hp <= 0) {
        return EffectResult::kOk;
    }
    if (!foe_executed_this_round(ctx, opp)) {
        return EffectResult::kOk;   // 对手没用技（NONE/被控）→ 不记录不判定
    }
    const int type = ctx->ws.skill_type_view[opp];
    K382Runtime& rt = k382_runtime(ctx, owner);
    if (rt.last_type >= 0) {
        if (type == rt.last_type) {
            rt.same_round = ctx->roundCount;   // 相同 → 下回合对手先制-1 戳
        } else {
            // 不同 → 100% 害怕 3 回合（短路申报不耗 rand）。
            // ⚠️ ROUND_END 时点补偿 +1（2026-10-24 接管实测）：advanceRound 在
            //   ROUND_COMPLETION，回合末申报 end=rc+3 会被本回合消费掉一格、
            //   实盖 2 个整回合——引擎 derive_anomaly 同相位即 rc+rounds+1。
            //   补偿后对手恰有 kFearDuration 个完整害怕回合。
            (void)g_core->apply_anomaly_checked(ctx, opp, kFearId, kFearDuration + 1,
                                                owner, 100, ChanceSource::SoulMark);
        }
    }
    rt.last_type = type;
    return EffectResult::kOk;
}

// ── ①' FMR：下回合对手先制 -1（戳消费，一次性）────────────────────────────────
EffectResult k382_2501_fmr(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    K382Runtime& rt = k382_runtime(ctx, owner);
    if (rt.same_round < 0 || rt.same_round != ctx->roundCount - 1) {
        return EffectResult::kOk;
    }
    rt.same_round = -1;   // 一次性消费
    const int opp = 1 - owner;
    ctx->ws.preemptive_level[opp] -= 1;
    return EffectResult::kOk;
}

// ── ② DETECT：攻击命中 + 害怕 → 回合-1 + 递增固伤 ────────────────────────────
constexpr int kWiredKeyBase = 990382000;   // +owner：DETECT 效果已挂旗标

EffectResult k382_2501_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // early 双触发防重（k189 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !g_core || !g_core->fixed_damage ||
                !g_core->set_anomaly_rounds) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤对账
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = 1 - owner;
            if (!foe_feared(c, opp)) {
                return;
            }
            int cap = 400;
            {
                const int slot = c->find_pet_with_soulmark(owner, kMarkId);
                if (slot >= 0 && slot <= 5) {
                    const auto& db =
                        c->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
                    if (db.size() > 1 && db[1] > 0) {
                        cap = db[1];
                    }
                }
            }
            K382Runtime& rt = k382_runtime(c, owner);
            int bonus = kBonusBase + kBonusStep * rt.atk_procs;
            if (bonus > cap) {
                bonus = cap;
            }
            shift_fear_rounds(c, opp, -1);
            (void)g_core->fixed_damage(c, opp, bonus);
            ++rt.atk_procs;
        });
    return EffectResult::kOk;
}

// ── ③ ON_SKILL_HIT：属性命中 + 害怕 → 回合+1 + 递减吸取 ──────────────────────
EffectResult k382_2501_on_skill_hit(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (used_attack_skill(ctx, owner)) {
        return EffectResult::kOk;   // ② 的攻击对账走 DETECT；本桶只认属性技命中
    }
    const int opp = 1 - owner;
    if (!foe_feared(ctx, opp)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int start_pct = db_at(row, 2, 40);
    const int floor_pct = db_at(row, 3, 20);
    K382Runtime& rt = k382_runtime(ctx, owner);
    int pct = start_pct - kDrainStepPct * rt.attr_procs;
    if (pct < floor_pct) {
        pct = floor_pct;
    }
    shift_fear_rounds(ctx, opp, +1);
    if (ctx->getPet(opp).hp > 0 && pct > 0) {
        int drain = resolve_pet_max_hp(ctx->getPet(opp)) * pct / 100;
        const int foe_hp = ctx->getPet(opp).hp;
        if (drain > foe_hp) {
            drain = foe_hp;   // 吸取不越过存量（K302/K338 口径）
        }
        if (drain > 0) {
            (void)g_core->deal_pink_damage(ctx, opp, drain, DamageKind::FIXED, owner);
            (void)g_core->heal_amount(ctx, owner, drain);
        }
    }
    ++rt.attr_procs;
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（2501 入账，只增不改）──────
// tips 尾注（boss无效）→ 全节点 boss_invalid=true。
// ⚠️ 五节点：FMR 戳消费节点（2026-10-24 接管补注册——前任写了 k382_2501_fmr
//   却漏挂 SOUL_NODE，T7291 先制-1 断言红；FMR 桶先 memset 清零再跑注册动作，
//   本节点写在清零后、先制比较前，落位正确）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k382_2501_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_ROUND_END, &k382_2501_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_MOVE_RIGHT, &k382_2501_fmr,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_ON_SKILL_HIT, &k382_2501_on_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_ON_SKILL_HIT, &k382_2501_on_skill_hit,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
