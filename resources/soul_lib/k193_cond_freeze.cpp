// K193 条件冰封族 —— effect 827（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 827，2026-10-07）：
//   827 霜焱主宰 3203 args[80, 1]：「若对手处于烧伤或冻伤状态，则攻击技能命中时使其
//     80%概率被冰封（BOSS无效）」/ 霜焱 3212 args[80, 1, 1]（row1 + 补子句：「若对手
//     不处于异常状态，则属性技能命中时消除对手回合类效果」，BOSS无效全句尾注）
//     → db[0]=冰封概率（80）、db[1]=boss 旗 datum（两行同值，boss_invalid 由 tips
//       尾注承载，arg 不消费——记档）、db[2]=补子句旗（row2 独有，在=补子句生效）。
//
// ⚠️ 实现形态（T4 类型化行参）：
//   clause1（293 直系）：enter arm 接线 DETECT 相位管线票（epoch 防重，ON_STAGE 绑定）
//     ——条件查询赶在状态活着的时候同步做（K1 SOP：TAKE_DAMAGE watcher 会迟到期
//     AFTER_ACTION）；门 = d.attackerId==owner + isRed + final>0 + 对手烧伤(2)|冻伤(5)
//     → apply_anomaly_checked(冰封15, chance=db[0], SoulMark)——**不预掷**（K191 双掷
//     勘误：申报通道内掷一次）。命中语义由 DETECT 相位天然承载（miss 无管线）。
//   clause2（row2 db[2]）：FIRST/SECOND_AFTER_ACTION 双桶节点（行动方桶跑行动方条目，
//     yanshou 记档）——E13(SKILL_USED) 在命中判定**前**发、结算结果 execute 返回后才
//     write，故命中实绩门锚 AFTER_ACTION 读 ws.skill_exec_result==HIT；roundChoice+
//     类型表门（属性技）+ 对手无异常门 → break_round_effects(对手)（1038 原语）。
//
// ⚠️ 三查记录（author 2026-10-07 / k-prod-2 复核同日）：①内核无 827 魂印行为硬编码
//    ②并集无同 id 注册 ③effect_info 无 827 技能模板——真缺口。
// 先例：k_cond_power_family 293（DETECT 同步条件查询全形态）、anomaly_channel（申报
//   通道）、yanshou（AFTER_ACTION 桶语义 + break_round_effects）。
// 场景回归：test/scenario/scenario_3245_cond_freeze.cpp（号段 3245-3249）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <effects/rule_center.h>

namespace {

// ── 宿主行参数（方案 B：db_at 读 owned_int_args **原序**，与真实对局一致）─────
// ⚠️ 历史沿革：本文件原用 a.row_or(i, …) 读 SoulNodeArgs.row，但 bind_soulmark_args
//    （soul_mark_manager.cpp:351）会丢弃 owned_int_args 前两位 → 真实对局里
//    a.row = DB args[2:]（多数情况为空）→ 读值错位/落 fallback。
//    2026-10-21 按方案 B 迁移为 db_at（读 owned_int_args 原序）；场景同步改为
//    按 DB args 原样构造（不再补 {0,0} 占位）。
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


constexpr int kMarkId = 827;
constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);      // 15
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);              // 2
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);    // 5

// arm 幂等守卫（k184 EpochGuard 私副本）：early 节点开局双触发防重。
struct EpochGuard {
    int armed_epoch = -1;
};

bool already_armed_this_epoch(BattleContext* ctx, int owner) {
    ElfPet& pet = ctx->getPet(owner);
    if (!pet.soulmark_storage.count(kMarkId)) {
        pet.soulmark_storage[kMarkId] = EpochGuard{};
    }
    EpochGuard& guard = std::any_cast<EpochGuard&>(pet.soulmark_storage[kMarkId]);
    if (guard.armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    guard.armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

// ── clause1 enter arm：DETECT 相位同步条件查询 → 冰封 ──
EffectResult k193_827_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || a.owner < 0 || a.owner > 1) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    const HostRow row = host_row(ctx, a, 827);
    const int chance = db_at(row, 0, 80);
    if (chance <= 0 || already_armed_this_epoch(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, chance](BattleContext* c, int bucket_owner) {
            if (!c || !g_core || !g_core->apply_anomaly_checked
                || !g_core->has_active_anomaly) {
                return;
            }
            if (bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;   // 只随自己的攻击红伤（命中语义由管线相位天然承载）
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int opp = c->opponent(owner);
            if (!g_core->has_active_anomaly(c, opp, kBurn)
                && !g_core->has_active_anomaly(c, opp, kFrostbite)) {
                return;   // 门：对手处于烧伤|冻伤（DETECT 相位同步查询）
            }
            (void)g_core->apply_anomaly_checked(c, opp, kIcebound,
                                                random_anomaly_duration(), owner,
                                                chance, ChanceSource::SoulMark);
        });
    return EffectResult::kOk;
}

// ── clause2 AFTER_ACTION 节点：属性技命中 + 对手无异常 → 消对手回合类 ──
EffectResult k193_827_after_action(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    if (db_at(row, 2, -1) < 0) {
        return EffectResult::kOk;   // 行无补子句旗（3203 行）→ 不设
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5
        || ctx->getPet(owner).skills[idx].type != SkillType::Attribute) {
        return EffectResult::kOk;   // 只认属性技（K4 判据：类型表门）
    }
    if (ctx->ws.skill_exec_result[owner] != SkillExecResult::HIT) {
        return EffectResult::kOk;   // 命中实绩门（E13 后投递，AFTER_ACTION 可见）
    }
    const int opp = ctx->opponent(owner);
    if (ctx->has_any_active_abnormal_status(opp)) {
        return EffectResult::kOk;   // 门：对手不处于异常状态
    }
    (void)g_core->break_round_effects(ctx, opp);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（827 入账，只增不改）────────
// BOSS无效 = 全句尾注（两行）→ 三节点统一置位。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k193_827_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_FIRST_AFTER_ACTION, &k193_827_after_action,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true),
              SOUL_NODE(State::BATTLE_SECOND_AFTER_ACTION, &k193_827_after_action,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
