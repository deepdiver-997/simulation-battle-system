// K306 攻技回合末自增族 —— effect 765（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 765，2026-10-21）：
//   765 龙翼战神 3104 args[1,1]（kind=[11]）：「所有攻击技能使自身在回合结束后
//     攻击+1，速度+1」
//     → **db[0]=1 = 攻击增量**、**db[1]=1 = 速度增量**（双参自证）。
//     ⚠️ 参数自证（同族行交叉锚定）：
//       751 args[1,1,1]「自身先出手时双防+1；后出手时攻击+1，速度+1」——
//       **db[1]=1（攻击+1）、db[2]=1（速度+1）**；本行两个子句与之一一对应
//       （无先手门 → 少一位），故 db[0]=攻击、db[1]=速度；
//       1752 args[3,1,3,1,1,3,1]「…令自身攻击+1，速度+1」——同款两增量。
//       → 本行 args[1,1] 与"攻击+1、速度+1"两子句自洽对应。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   触发 = **本回合使用了攻击技**（tips「所有攻击技能使自身在回合结束后…」——
//     增益挂在"用了攻击技"这个条件上，且**结算时点是回合结束后**）。
//   时点 = BATTLE_ROUND_END（"回合结束后"字面；K178 351「战阶结束」、K163 261
//     「每回合恢复1/10最大体力」同款 ROUND_END 桶）。
//   门 = 本回合用过攻击技。判定用 **ws.skill_type_view / skill_used** 面：
//     ⚠️ **不能用 roundChoice 读**——ROUND_END 时选择期已过、roundChoice 被清
//       （battleFsm.cpp:2325 回合结束清 roundChoice）。
//     改用 `ws.skill_used[owner]`（本回合是否出手）+ `ws.skill_type_view[owner]`
//     （本次技能的类型**视图**——存在技能转换时以视图为准，K165/K212 同款读法）。
//   效果 = stat_change(自身, 槽0=攻击, +db[0]) + stat_change(自身, 槽4=速度, +db[1])
//     （**自身增益**通道——stat_change 写**真实 pet.levels（持久）**，不受 ws 每回合
//     reset 影响；**不查免弱**（stat_drop 才是查免弱的那个）。K4 622 自增益同款）。
//     槽序（速查 §三）：**0攻 4速**。
//     ⚠️ 等级上限 ±6 由原语内钳（stat_change 自钳 AT_CAP）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 765 无魂印行为硬编码；
//    ②effect_icon kind=[11]、tips 为权威文（dump 原文已核；effect_info 无 765
//    技能模板）；③registry soul 集合无 765、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k_round_stat 622（stat_change 自增益通道，直系）、K178 351（ROUND_END 桶）、
//   k165_gate_attach_power（skill_type_view 视图读法）、K163 261（ROUND_END 回血）。
// 场景回归：test/scenario/scenario_4620_atk_roundend_boost_765.cpp（号段 4620-4629）。

#include "common.h"

#include <entities/skills.h>

namespace {

constexpr int kMarkId = 765;
constexpr int kAtkSlot = 0;   // 槽 0 = 攻击
constexpr int kSpeedSlot = 4; // 槽 4 = 速度

// ── 宿主行参数（db_at 原序读 owned_int_args——真实对局与场景语义一致）────────
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

// ── 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h，就地复刻）─────
// ⚠️ 抄 moves_lib/common.h 的 executed_skill_this_round / executed_skill_is_attack：
//   · 「本回合是否**成功执行过**技能」：skill_use_seq（resolve_skill_execution 主
//     流程 ++；被控/嗑药/NONE **不动**；miss/被盔算使用）较回合初基线
//     （ws.skill_use_seq_round_start，FSM 回合初快照）有增量。
//   · 「本次执行的技能是不是攻击技」：读 ws.skill_type_view——每次使用物化**实际
//     执行技能**的类型（替换技/变型技正确）。
//   ⚠️ 不用 ws.skill_used（它是"出手过"标记，被控/嗑药也会置位）；不用 roundChoice
//     （ROUND_END 时选择期已过、roundChoice 被清，battleFsm.cpp:2325）。
bool executed_skill_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_skill_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

// ROUND_END：本回合用过攻击技 → 自身攻击 +db[0]、速度 +db[1]。
EffectResult k306_765_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // 门：本回合是否**成功执行过技能** + 本次执行的技能是不是攻击技
    // （判据统一 §四点五 私副本，见上）。
    if (!executed_skill_this_round(ctx, owner)) {
        return EffectResult::kOk;   // 本回合没成功用技能（被控/嗑药/NONE 不算）
    }
    if (!executed_skill_is_attack(ctx, owner)) {
        return EffectResult::kOk;   // 属性技不触发（tips"所有攻击技能"）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int atk_delta = db_at(row, 0, 1);   // db[0] = 1（攻击+1）
    const int speed_delta = db_at(row, 1, 1); // db[1] = 1（速度+1）
    if (atk_delta > 0) {
        (void)g_core->stat_change(ctx, owner, kAtkSlot, atk_delta);
    }
    if (speed_delta > 0) {
        (void)g_core->stat_change(ctx, owner, kSpeedSlot, speed_delta);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（765 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_END, &k306_765_round_end,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
