// K277 回合回血低体翻倍族 —— effect 610（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 610，2026-10-21）：
//   610 昊希娜 2849 args[5,3]（kind=[5]）：「每回合恢复1/5体力，体力少于1/3时恢复
//     效果翻倍」
//     → **db[0] = 1/5 的分母**（tips 字面 "1/5体力" ↔ 5）；**db[1] = 1/3 的门限分母**
//       （tips 字面 "体力少于1/3" ↔ 3）。两参数与文本**完全自洽** → 均消费。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   时点 = **BATTLE_ROUND_START**（"每回合"——K10 637 / K 板 359 / K 板 368 同款
//     回合首节点；声明节点每回合由更新器重注册，SOP §一.1）。
//   回血量 = resolve_pet_max_hp(自己) / db[0]（**最大体力的 1/db[0]**，不是当前
//     体力——K10 637 的 heal(max/分母) 同款口径；"恢复1/5体力"按 K 板 351「减少
//     1/5最大HP」同族口径取最大体力）。
//   翻倍门 = **自身体力** < 最大体力的 1/db[1]（严格小于，官方"体力少于1/3"；
//     判据 hp * db[1] < max，整数式避免除法截断）。过门 → 回血量 ×2。
//   效果 = heal_amount(自己, 回血量)（吃封回血/恢复修正/满血封顶——K10 637 同款
//     原语调用；原语返回 HealResult，失败（封回血）不另作处理）。
//   ⚠️ 满血封顶：满血时 heal_amount 归零无效——场景断言须**先压血量**
//     （SOP §五.2「压血防封顶」）。
//
// ⚠️ 三查记录（author a08-kprod 2026-10-21）：①内核 grep 610 无魂印行为硬编码
//    ——src/entities/skills.cpp:509/520 的 610 是**技能空间**条件先制路由
//    （"610 璨灵圣光：遇到天敌时先制+{0}"），**同号异实**、不同注册空间，不算
//    魂印已实现（记档）；②effect_icon kind=[5]、tips 为权威文（dump 原文已核，
//    effect_info 无 610 技能模板）；③registry soul 集合无 610、soul_lib 全库
//    grep 无同 id 注册——真缺口。
// 先例：k_round_recover_family 637（ROUND_START heal + db 原序读）、K 板 359
//   （ROUND_END 磨血）、K10 227（登场计数器）。
// 场景回归：test/scenario/scenario_4330_round_heal_double_610.cpp（号段 4330-4339）。

#include "common.h"

namespace {

constexpr int kMarkId = 610;
constexpr int kHealDenomFallback = 5;   // tips 字面 1/5 的分母（db[0] 读不到时回退）
constexpr int kGateDenomFallback = 3;   // tips 字面 1/3 的门限分母（db[1] 读不到时回退）
constexpr int kDoubleMult = 2;         // tips 字面"翻倍"

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

// ROUND_START：每回合恢复 maxHP/db[0]；体力 < maxHP/db[1] 时翻倍。
EffectResult k277_610_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal_denom = db_at(row, 0, kHealDenomFallback);
    const int gate_denom = db_at(row, 1, kGateDenomFallback);
    if (heal_denom <= 0 || gate_denom <= 0) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 恢复不是重生（合并 K281 的存活守卫）。
    }
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    int heal = max_hp / heal_denom;
    if (heal <= 0) {
        return EffectResult::kOk;
    }
    // 翻倍门：自身体力 < 最大体力的 1/gate_denom（严格小于；整数式免除法截断）。
    if (static_cast<long long>(self.hp) * gate_denom < max_hp) {
        heal = heal * kDoubleMult;
    }
    (void)g_core->heal_amount(ctx, owner, heal);
    return EffectResult::kOk;
}

}  // namespace

// ── 唯一注册（610；K277/K281 同行实现已收敛，保留唯一ID覆盖）─────────────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k277_610_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
