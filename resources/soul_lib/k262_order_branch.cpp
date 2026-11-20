// K262 先后手双分支族 —— effect 369（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 369，2026-10-21）：
//   369 妖皇兰蒂斯 2777 args[3,20]（kind=[1]）：「若对手先出手则立即减少1/3HP
//     （boss无效）；若自己先出手则造成的伤害提升20%」
//     → **db[0] = 1/3 的分母**（tips 字面 "1/3" ↔ db[0]=3，与 K 板 351「减少1/5
//       最大HP」args[1000,5] 的 db[1]=5=分母 同款分母位口径）；**db[1] = 增伤 20%**
//       （tips 字面 ↔ db[1]=20，K240 776 增伤通道 db[0]=50 同款）。两参数与文本
//       **完全自洽**（本夜少见的自洽行）→ 两者均消费。
//     ⚠️ 口径记档：「减少1/3HP」的"1/3"基数存疑——按 K 板 351「减少1/5**最大HP**」
//       同族口径取**最大HP**的 1/3（粉伤 FIXED）；若官方实为"当前HP的 1/3"，
//       改本文件一处即可。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ①arm 节点（early，幂等旗）：挂 **DETECT 相位**零状态票（yanshou 附伤通道，
//     K169 393 先手攻附固伤同款）——每次红伤结算时同步读**已结算**的先手权：
//       · 子句①（对手先出手 → 立即减少对手 1/3 HP）：
//         门 = is_second_mover(owner) 为真（**自己是后手 = 对手先手**）且
//         本次红伤的攻击方是对手（d.attackerId == opp）→ 扣对手
//         maxHP/db[0] 点固定伤害（deal_pink_damage FIXED，K 板 351 同款）。
//         ⚠️ "立即"口径 = 对手攻击落地的当刻（DETECT 在伤害管线内，同步执行）。
//       · 子句②（自己先出手 → 造成的伤害提升 20%）：
//         门 = 自己先手（!is_second_mover(owner)）且 d.attackerId == owner
//         → d.final = d.final * (100 + db[1]) / 100。
//         ⚠️ **增伤用 ws 钦定通道还是直接改 final**：本处**直接改 final**（DETECT
//         相位内，K169 393 攻附固伤同款），不走 ws.damage_add_pct——后者是 AMP
//         相位加算槽、要求在**攻击方自己的**时机写（K240 776 用法），而本子句
//         的门读的是"自己是否先手"（攻击时点已定），改 final 更直接（同 293 族）。
//   ⚠️ boss 门：tips 尾注带（boss无效）→ 节点 boss_invalid 置位（boss 挑战对局
//     不注册不执行）。
//   ⚠️ pr 读取时点：DETECT 相位在伤害管线内 → 此时 MOVE_RIGHT 早已结算、
//     ws.preemptive_right 有效（K235 705 记档：FMR 桶内读恒 NONE，伤害时点读
//     早已结算）→ is_second_mover 可直接读。
//
// ⚠️ 三查记录（author a01-kprod 2026-10-21）：①内核 grep 369 无魂印行为硬编码；
//    ②effect_icon kind=[1]、tips 为权威文（dump 原文已核，effect_info 无 369
//    技能模板）；③registry soul 集合无 369、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k169_order_branch 393（先/后手双分支 + DETECT 攻附固伤票，旗舰）、
//   k178_battle_stage_grind 351（减少 1/db[1] 最大HP 的粉伤 FIXED 口径）、
//   k240_streak_amp_break 776（增伤通道）、k235 705（pr 读取时点记档）。
// 场景回归：test/scenario/scenario_4180_order_branch_369.cpp（号段 4180-4189）。

#include "common.h"

namespace {

constexpr int kMarkId = 369;
constexpr int kDenomFallback = 3;    // tips 字面 1/3 的分母（db[0] 读不到时回退）
constexpr int kAmpFallback = 20;     // tips 字面增伤 20%（db[1] 读不到时回退）
constexpr int kWiredKeyBase = 990262000;   // +owner：DETECT 票已接线旗标

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

// arm：挂 DETECT 相位票（先手增伤 / 后手扣对手 1/3 HP）。
EffectResult k262_369_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int wired_key = kWiredKeyBase + owner;
    if (ctx->plugin_storage.count(wired_key)) {
        return EffectResult::kOk;   // early 双触发防重
    }
    ctx->plugin_storage.emplace(wired_key, 1);

    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 0, kDenomFallback);
    const int amp = db_at(row, 1, kAmpFallback);
    if (denom <= 0 || amp <= 0) {
        return EffectResult::kOk;
    }

    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, denom, amp](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner || !onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (!d.isRed || d.final <= 0) {
                return;
            }
            const int opp = c->opponent(owner);
            if (d.attackerId == owner) {
                // 子句②：自己先出手 → 造成的伤害提升 db[1]%。
                if (c->is_second_mover(owner)) {
                    return;   // 自己是后手 → 不增伤
                }
                d.final = d.final * (100 + amp) / 100;
                return;
            }
            if (d.attackerId == opp) {
                // 子句①：对手先出手（= 自己是后手）→ 立即减少对手 1/db[0] 最大HP。
                if (!c->is_second_mover(owner)) {
                    return;   // 自己先手 → 对手不是先手
                }
                ElfPet& foe = c->getPet(opp);
                if (foe.hp <= 0) {
                    return;
                }
                const int grind = resolve_pet_max_hp(foe) / denom;
                if (grind <= 0) {
                    return;
                }
                if (g_core && g_core->deal_pink_damage) {
                    (void)g_core->deal_pink_damage(c, opp, grind, DamageKind::FIXED, owner);
                }
                return;
            }
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（369 入账，只增不改）──────
// （boss无效）尾注 → boss_invalid 置位。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k262_369_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/true)});
