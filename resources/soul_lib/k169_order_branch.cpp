// K169 先/后手双分支族 —— effect 393（1 宏 / 2 行）
//
// 行清单（tools/soul_effects_dump.py 393，2026-10-06）：
//   393 奥古斯特 2819 args[150, 100]：「后出手时恢复150点体力，先出手时所有攻击
//     附加100点固定伤害」/ 地葬界王 2991 args[200, 200]：「先出手时附加200点固定
//     伤害;后出手恢复200HP体力」
//     → db[0]=后手回血量、db[1]=先手攻附固伤量（两行同构互锚）。
//     每回合按实际出手序分支：先手回合 → 己方攻击 +Y 固定（DETECT 相位
//     d.final += Y，攻击时点 ws.preemptive_right 已结算可读）；后手回合 → 
//     heal_amount(db[0])（ROUND_COMPLETION 节点，pr 该时点仍为当回合值——
//     K12 310 记档：pr 在次回合 FMR 才重置）。
//
// ⚠️ 实现形态：ENTER early arm 接线永久 DETECT 零状态票（先手攻附，实时读
//    preemptive_right）+ RC 声明节点（后手回血）。两分支互斥于当回合出手序。
//
// ⚠️ 三查记录（2026-10-06）：①内核无 393 魂印行为硬编码 ②effect_info 无 393 技能
//    模板 ③soul_lib 并集无同 id 注册——真缺口。
// ⚠️ 回收收尾（2026-10-07，k-prod-1）：原会话草稿未构建即中断；复核修正三处——
//    ①先/后手门改内核 is_second_mover 助手（原手搓 pr 枚举映射等价）；
//    ②DETECT 闭包补 onstage_has_soul_mark 在场门（桶 TEAM 绑定，换场不附加）；
//    ③plugin_storage 键回归 int。通道记档：工单建议 EVENT_SKILL_USED+fixed_damage，
//    实取 DETECT 相位 d.final += db[1]——管线注释明载 DETECT=「受击检测/触发
//    （反伤、附伤）」，附伤即此相位，且 miss（无伤害落地）不误附加。
// 先例：173/147（acting_side_is_first 门）、yanshou（DETECT 管线票）、K12 310（RC 读 pr）。
// 场景回归：test/scenario/scenario_3030_order_branch.cpp
//（⚠️ 号段记档：看板行 3030-3039 由 author 分配器实配（本单看板行即写此段），无撞号。）

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kWiredBase = 990169000;   // +宏*10+owner：管线已接线旗

// 宿主行解析（K2 host_row 私副本）。
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

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const int key = kWiredBase + macro * 10 + owner;
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

// ── ENTER early arm：接线 DETECT 零状态票（先手回合己方攻击 +db[1] 固定）──
EffectResult k169_393_arm(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 393, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 393);
    const int attach = db_at(row, 1, 100);
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, attach](BattleContext* c, int bucket_owner) {
            if (bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, 393)) {
                return;   // 换下场不附加（DETECT 桶 TEAM 绑定，需现场查在场）
            }
            // 先手门：本回合我先出手（pr 攻击时点已结算；NONE=未结算按先手）
            if (!c->is_second_mover(owner)) {
                d.final += attach;
            }
        });
    return EffectResult::kOk;
}

// ── ROUND_COMPLETION 节点：后手回合 → 回 db[0] 体力 ──
EffectResult k169_393_round_completion(BattleContext* ctx, const EffectArgs& args) {
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 393)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 393);
    const int heal = db_at(row, 0, 100);
    // 后手门：当回合出手序（pr 未重置，is_second_mover 读 ws 现值）
    if (ctx->is_second_mover(owner)) {
        (void)g_core->heal_amount(ctx, owner, heal);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（只增不改）────────────────
SOUL_PROGRAM(393, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k169_393_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_COMPLETION,
                                   &k169_393_round_completion)});
