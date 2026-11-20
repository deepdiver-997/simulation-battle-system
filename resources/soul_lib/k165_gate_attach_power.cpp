// K165 状态门附加/威力族 —— effect 303/291（2 宏 / 4 行）
//
// 行清单（tools/soul_effects_dump.py 303 291，2026-10-05）：
//   303 绝影·无邪 2605 args[2, 100] / 奥丁 2647 args[3, 100]：
//     "对手处于能力下降 附加100点伤害" / "若对手处于异常状态攻击附加100点固定伤害"
//     → db[0]=门类型（2=对手任一负槽、3=对手任意异常——两行互锚记档）、db[1]=固伤。
//     own 攻击技（EVENT_SKILL_USED + !used_attribute，K2 358 同门）→ 门判 →
//     fixed_damage(db[1])。对手门判：2=ability_levels 任一 <0；3=has_active_anomaly。
//   291 凯尔大帝 2548 args[] / 至尊无天 2734 args[50, 0]：
//     "对手处于异常状态时威力翻倍" / "对手处于异常状态时造成的伤害提升50%"
//     → own 攻击技 + 对手异常 → skill_power_view 条件写（K4 636「对手异常时威力翻倍」
//     1443 变威力管线同路）：零参行 ×2；db 行 ×(100+db[0])/100。
//
// ⚠️ 实现形态：EVENT_SKILL_USED watcher（own + 攻击技 + onstage 门）——事件面不受
//    更新器重建影响（K6 记档）；wired-once。对手门判在攻击时点实时读（含回合内变化）。
//
// ⚠️ 三查记录（2026-10-05）：①内核无 303/291 魂印行为硬编码 ②effect_info 无此二 id
//    技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：K2 358（SKILL_USED 攻击门 + apply）、K4 636（对手异常 → power_view 翻倍）、
//   K2 262（TAKE_DAMAGE watcher 形）。
// 场景回归：test/scenario/scenario_2950_gate_attach_power.cpp（号段 2950-2959；
//   ⚠️ 看板行段号 2880-2889 与 K163 撞号（并行 author 竞态），已改 2950-2959——
//   看板行不回改，以本记档对账）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kWiredBase = 990165000;   // +宏*10+owner：watcher 已挂旗

// 判据统一 §四点五 私副本（soul 侧不链 moves_lib/common.h）。
bool executed_this_round(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->skill_use_seq[owner] > ctx->ws.skill_use_seq_round_start[owner];
}

bool executed_is_attack(const BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return false;
    }
    return ctx->ws.skill_type_view[owner] != static_cast<int>(SkillType::Attribute);
}

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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

// SoulNodeArgs 版（类型化节点用：owner/host_slot 直接来自行参）—— 方案 B 迁移新增。
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
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const auto key = static_cast<unsigned long long>(kWiredBase + macro * 10 + owner);
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

bool opp_gate_drop(BattleContext* ctx, int opp) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            return true;
        }
    }
    return false;
}



// own 攻击技事件判据（K2 self_attack_skill_event 同构）。
bool own_attack_event(BattleContext* ctx, int owner, const BattleEvent& ev, int mark_id) {
    if (ev.actor != owner || ev.used_attribute) {
        return false;
    }
    return onstage_has_soul_mark(ctx, owner, mark_id);
}

// ── 303 watcher：own 攻击技 → 对手门（负槽/异常）→ 附加固伤 ──
EffectResult k165_303_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 303, owner)) {
        return EffectResult::kOk;
    }
    // 行参走 db_at（方案 B：读 owned_int_args **原序**，与真实对局一致）：
    // db[0]=门类型、db[1]=固伤（官方行原序）。
    const HostRow row = host_row(ctx, a, 303);
    const int gate_type = db_at(row, 0, 2);
    const int fixed = db_at(row, 1, 100);
    const int opp = a.opp;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, opp, gate_type, fixed](BattleContext* wctx, const BattleEvent& ev) {
                    if (!wctx || !own_attack_event(wctx, owner, ev, 303)) {
                return;
            }
            const bool gate = (gate_type == 3) ? wctx->has_any_active_abnormal_status(opp)
                                               : opp_gate_drop(wctx, opp);
                    if (!gate || !g_core || !g_core->fixed_damage) {
                return;
            }
            (void)g_core->fixed_damage(wctx, opp, fixed);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 291 SKILL_EFFECT 节点（FIRST/SECOND 双桶，持有方自身行动时触发）：对手异常 →
//    power_view 增。照抄 K4 636 形态：roundChoice 门 + 技能表类型门（该时点
//    skill_use_seq 尚未自增，executed_* 不可读）；写点=物化后、伤害公式前。
//    行参从绑定后 args[3:] 读（{0,0} 占位后无参 → 翻倍 +100）。
EffectResult k165_291_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 291)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    const int idx = ctx->roundChoice[owner][1];
    if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
        return EffectResult::kOk;   // 只翻攻击技的威力
    }
    if (!ctx->has_any_active_abnormal_status(1 - owner)) {
        return EffectResult::kOk;   // 门：对手处于异常状态
    }
    // 行参走 db_at（方案 B）：db[0]=bonus（{50,0} 行 → 50%；零参行 → 翻倍）。
    const HostRow row = host_row(ctx, a, 291);
    const int bonus = db_at(row, 0, 100);
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * (100 + bonus) / 100;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（只增不改）────────────────
// T4：节点函数已迁移 SoulNodeArgs 类型化行参，注册走 SOUL_NODE()（编译期防错位）。
SOUL_PROGRAM(303, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k165_303_arm,
                   /*once=*/false, /*early=*/true)});
SOUL_PROGRAM(291, {SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT,
                   &k165_291_skill_effect),
                   SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT,
                             &k165_291_skill_effect)});
