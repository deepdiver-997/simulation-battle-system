// K279 致命一击回血族 —— effect 601（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 601，2026-10-21）：
//   601 天绝·狂煞 2834 args[2,2]（kind=[5]）：「打出致命一击时恢复1/2最大HP」
//     → **db[0]=2 = 最大HP分母**（tips 字面"1/2"自证）；db[1]=2 语义存疑不消费（记档）。
//       单行唯一解：tips 只有一个数值子句（1/2 最大HP），args[0]=2 与之自洽。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   门 = **打出致命一击**（K15 133 口径：致命一击 = 暴击，引擎 `DamageSnapshot.isCrit`；
//     非"瞬杀/秒杀"——后者无伤害量）。三重条件：
//       ①attackerId == owner（自己打出的暴击，不是对手打我的）；
//       ②isRed（红伤；属性直伤/粉伤/真伤不算"打出致命一击"——K15 133 同款口径）；
//       ③isCrit（暴击命中）+ final > 0（伤害未被归零——被挡下的暴击不算"打出"）。
//   时点 = **DETECT 相位**（伤害管线内，K194 619 直系同款）。⚠️ DETECT 时点
//     `final` 已定（含暴击加成）且**落血尚未发生**（deal_damage 在 apply_resolved_damage
//     里、管线之后）——K194 619 吸血同款口径："DETECT 时点 final 已定含暴击、hp 未扣"。
//   效果 = heal_amount(owner, resolve_pet_max_hp(印主) / db[0])（**自身回血**，
//     K163 261 / K10 637 同款通道；吃封回血钳 max——K194 619 同款）。
//     ⚠️ 扣的是**印主自己**的最大HP（"恢复1/2最大HP"的主语=自己，K163 261
//     "每回合恢复1/10最大体力"同款读法）。
//   ⚠️ 不走 EVENT_TAKE_DAMAGE watcher：watcher 交付被推迟到 AFTER_ACTION，且
//     "对手受到伤害"事件不携带"是攻击方打的暴击"这一层信息（actor 是防守方视角）；
//     DETECT 相位直接读 resolvedDamage 是 K 系攻击方效果的定式（K194/K168/K189）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 601 无魂印行为硬编码；
//    ②effect_icon kind=[5]、tips 为权威文（dump 原文已核；effect_info 无 601
//    技能模板）；③registry soul 集合无 601、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k194_crit_drain 619（**致命一击/暴击 + DETECT 相位伤害回血，旗舰直系**）、
//   k_guard_family 150/133（"致命一击"= 暴击 isCrit 门，K15 口径）、
//   k168_damage_heal 363（DETECT 相位伤害回血，K194 直系同款）。
// 场景回归：test/scenario/scenario_4350_crit_heal_601.cpp（号段 4350-4359）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 601;
constexpr int kDenomFallback = 2;   // tips 字面"1/2最大HP"（db[0] 读不到时兜底）

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

// DETECT 相位：自己打出的暴击 → 恢复自身 1/db[0] 最大HP。
EffectResult k279_601_detect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int denom = db_at(row, 0, kDenomFallback);   // db[0] = 2（"1/2最大HP"，tips 自证）
    if (denom <= 0) {
        return EffectResult::kOk;
    }
    // 装票：DETECT 相位随每次攻击结算跑一次，命中暴击门即回血。
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, denom](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            // ①自己打出 ②红伤 ③暴击 ④伤害未被归零。
            if (d.attackerId != owner || !d.isRed || !d.isCrit || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const ElfPet& self = c->seerRobot[owner].elfPets[c->on_stage[owner]];
            const int max_hp = resolve_pet_max_hp(self);
            if (max_hp <= 0) {
                return;
            }
            (void)g_core->heal_amount(c, owner, max_hp / denom);
        });
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（601 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k279_601_detect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
