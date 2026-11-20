// K320 双防和比例固伤族 —— effect 694（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 694，2026-10-21）：
//   694 奇拉美 2986 args[1,30]（kind=[14]）：「攻击附加对手双防值之和30%的固定伤害」
//     → **db[1]=30 = 固伤百分比**（tips 字面"30%"自证）；db[0]=1 语义存疑不消费
//       （记档）。
//     ⚠️ 参数自证（同族行交叉锚定）：**同系同款** K233 844 args[50,50,1] /
//       [60,60,1]「攻击附加对手双防之和50%/60%的固定伤害」——百分比参数存在**第 0 位**
//       （844 主体行 db[0]=50、伙伴行 db[0]=60，与 tips 字面一一对应）；本行
//       args=[1,30] 的百分比 30 落在**第 1 位**——**布局与 844 不同**（844 有两个
//       子句占前两位：db[0]=附加%、db[1]=回血%；本行只有"附加"一个子句 → 百分比
//       独占 db[1]、db[0]=1 是废位/标记）。按 K4 头注「k_allowed 布局互不通用」
//       与 K2 纪律「解析不了宁可不用、不猜」→ **只消费 db[1]**。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   触发 = **自己攻击对手**且造成伤害（tips「攻击附加…」；K233 844 直系同款
//     EVENT_TAKE_DAMAGE watcher：ev.actor == 自己、ev.amount > 0、且
//     **ev.state ∈ {FIRST/SECOND_ATTACK_DAMAGE}**——只认**攻击伤害**时点，固定/
//     真伤/粉伤不构成"攻击"（K1 766 口径））。
//   效果 = deal_pink_damage(对手, def_sum(对手) * db[1] / 100, DamageKind::FIXED,
//     印主)——**固定伤害 = FIXED 档粉伤**（K233 844 明确口径：吃护罩/粉伤免减，
//     不吃护盾）。
//     ⚠️ **"对手双防值之和"** = 对手的 `numericalBase[DEFENSE] +
//       numericalBase[SPECIAL_DEFENSE]`（K233 844 已有 `def_sum` 助手同款口径：
//       读**基础数值**的防御+特防，不含能力等级加成）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 694 无魂印行为硬编码；
//    ②effect_icon kind=[14]、tips 为权威文（dump 原文已核；effect_info 无 694
//    技能模板）；③registry soul 集合无 694、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：k233_dualdef_fixed_heal 844（**同系同款：双防和比例固伤 + def_sum 助手 +
//   攻击伤害时点门，直系**——本单是它的"只有附加半边"版）。
// 场景回归：test/scenario/scenario_4760_defsum_fixed_694.cpp（号段 4760-4769）。

#include "common.h"

#include <entities/numerical-properties.h>

namespace {

constexpr int kMarkId = 694;
constexpr int kPctFallback = 30;   // tips 字面"30%"（db[1]）

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

// 对手"双防值之和"——K233 844 同款口径（基础数值的防御 + 特防，不含等级加成）。
int def_sum(const ElfPet& pet) {
    return pet.numericalBase[NumericalPropertyIndex::DEFENSE]
         + pet.numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
}

// arm：挂 EVENT_TAKE_DAMAGE watcher（自己攻击打中 → 附加 双防和×db[1]% 固伤）。
EffectResult k320_694_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = row.valid ? db_at(row, 1, kPctFallback) : kPctFallback;
    if (pct <= 0) {
        return EffectResult::kOk;   // 无参不武装（记档）
    }
    const int key = 990694000 + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // arm-once 防重
    }
    ctx->plugin_storage.emplace(key, 1);
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [owner, pct](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.actor != owner || ev.amount <= 0) {
                return;   // 只认"自己攻击对手"造成的伤害（K233 844 同款）
            }
            if (!onstage_has_soul_mark(wctx, owner, kMarkId)) {
                return;
            }
            // 只认**攻击伤害**时点（K1 766 口径：固定/真伤/粉伤不构成"攻击"）。
            // ⚠️ **这道门同时防住"固伤递归放大"**（实测记档）：本 watcher 挂的是
            //    EVENT_TAKE_DAMAGE——自己附加的**粉伤**也会发该事件；若不限时点，
            //    附加的固伤会再次触发本 watcher → 无限递归放大（变异 MUT2 实测对手
            //    掉血 484 而非 64）。限定 ev.state ∈ {FIRST/SECOND_ATTACK_DAMAGE}
            //    即只认**红伤**攻击结算时点，粉伤不构成"攻击"→ 不会自触发。
            if (ev.state != static_cast<int>(State::BATTLE_FIRST_ATTACK_DAMAGE)
                && ev.state != static_cast<int>(State::BATTLE_SECOND_ATTACK_DAMAGE)) {
                return;
            }
            const int opp = 1 - owner;
            const ElfPet& foe = wctx->getPet(opp);
            if (foe.hp <= 0) {
                return;
            }
            const int amount = def_sum(foe) * pct / 100;
            if (amount > 0) {
                (void)g_core->deal_pink_damage(wctx, opp, amount,
                                               DamageKind::FIXED, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（694 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k320_694_arm,
                        /*once=*/false, /*early=*/true, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
