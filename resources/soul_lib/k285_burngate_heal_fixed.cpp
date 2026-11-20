// K285 烧伤门回血/固伤族 —— effect 618（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 618，2026-10-21）：
//   618 弗雷姆 2869 args[2,200,200]（kind=[5,14]）：「若对手处于烧伤状态时则每回合
//     恢复200HP；若对手不处于烧伤状态则每回合附加200点固定伤害」
//     → **db[1] = 回血量 200**（tips 字面 "恢复200HP" ↔ 200）；**db[2] = 固伤量
//       200**（tips 字面 "附加200点固定伤害" ↔ 200）。两值自洽（双 200）→ 均消费。
//     db[0]=2 语义存疑不消费、记档（tips 无数值子句与之对应）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = **BATTLE_ROUND_START**（"每回合"——K255 320 / K268 796 同款回合首
//     节点；声明节点每回合由更新器重注册，SOP §一.1）。
//   门 = 对手处于烧伤状态 → `has_active_anomaly(opp, Burn=2)`（现代异常通道的
//     条件查询面；K255 320 / K254 299 同款判据）。
//   两分支（互斥）：
//     · 对手**烧伤** → heal_amount(自己, db[1])（"每回合恢复200HP"——K277 610 /
//       K10 637 同款原语：吃封回血/恢复修正/满血封顶）。
//     · 对手**不烧伤** → deal_pink_damage(对手, db[2], FIXED)（"每回合附加200点
//       固定伤害"——"固定伤害"= FIXED 档粉伤，K233 844 / K260 332 同款口径；
//       粉伤吃护罩）。
//   ⚠️ 边界：对手已倒（hp<=0）时固伤空转（K260 332 / K268 796 同款守卫）。
//
// ⚠️ 三查记录（author a11-kprod 2026-10-21）：①内核 grep 618 无魂印行为硬编码；
//    ②effect_icon kind=[5,14]、tips 为权威文（dump 原文已核，effect_info 无 618
//    技能模板）；③registry soul 集合无 618、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：K255 320（异常门 + 回合节点）、K277 610（ROUND_START heal）、
//   K260 332（ROUND_END 粉伤 FIXED）、K233 844（"固定伤害"= FIXED 档口径）。
// 场景回归：test/scenario/scenario_4410_burngate_heal_fixed_618.cpp（号段 4410-4419）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 618;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kHealFallback = 200;    // tips 字面"恢复200HP"（db[1] 读不到时回退）
constexpr int kFixedFallback = 200;   // tips 字面"附加200点固定伤害"（db[2] 读不到时回退）

// 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）。
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

// ROUND_START：对手烧伤 → 自己回血 db[1]；对手不烧伤 → 对手固伤 db[2]。
EffectResult k285_618_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->has_active_anomaly || !g_core->heal_amount
        || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal = db_at(row, 1, kHealFallback);
    const int fixed = db_at(row, 2, kFixedFallback);
    const int opp = ctx->opponent(owner);
    if (g_core->has_active_anomaly(ctx, opp, kBurn)) {
        // 分支①：对手烧伤 → 每回合恢复 db[1] HP。
        if (heal > 0) {
            (void)g_core->heal_amount(ctx, owner, heal);
        }
        return EffectResult::kOk;
    }
    // 分支②：对手不烧伤 → 每回合附加 db[2] 点固定伤害。
    if (fixed <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：固伤空转（K260 332 同款守卫）
    }
    (void)g_core->deal_pink_damage(ctx, opp, fixed, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

}  // namespace

// ── 唯一注册（618；K278/K285 同行实现已收敛，保留唯一ID覆盖）─────────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k285_618_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
