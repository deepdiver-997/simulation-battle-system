// K295 受伤减免+害怕门威力族 —— effect 613（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 613，2026-10-21）：
//   613 达克斯特 2857 args[20,6,50]（kind=[1,3]）：「受到的伤害减少20%，对手处于害怕
//     状态时所有技能威力提升50%」
//     → **db[0]=20 = 伤害减少%**（tips 字面"减少20%"自证）；
//       **db[1]=6 = 门异常码**（官方码 Fear=6，"害怕"字面自证）；
//       **db[2]=50 = 威力提升%**（tips 字面"提升50%"自证）。
//     ⚠️ 三参全自洽、无废位（与 K290 652「db[0]=门码/db[1]=倍数/db[2]=回血」的
//       布局**不同**——两行同族但布局互异，按本行 tips 字面一一对应取值）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   ①「受到的伤害减少20%」= ws.damage_reduce_add[owner]（**加算减伤百分比**，
//      REDUCE_PCT 阶段消费：Calculation::applyDamageReduction 把 4 槽**求和**后
//      final×(1-sum/100)）→ 填**首个空槽**（diwang_3512 死亡传承同款写法：4 槽是
//      并列加算项，求和等价，填第一个空槽即可）。
//      时点 = BATTLE_ROUND_START（ws 每回合 reset → 每回合重写；diwang 同款）。
//      ⚠️ 官方"受到的伤害"未分类型 → 本单按**红伤减伤**落（减伤槽的通用口径，
//         记档此选择）；粉伤（固定/百分比）另走 pink 管线，本单不覆盖。
//   ②「对手处于害怕状态时所有技能威力提升50%」= SKILL_EFFECT 时点
//      （K4 636 直系：威力视图改写必须在此——SKILL_USED 太早 view 未物化）：
//      对手有 db[1] 异常 → ws.skill_power_view[owner] ×(100+db[2])/100
//      （K290 652 同路 / K205 325 / K165 291 同款赋值形，rewritten 管线收尾重算）。
//      ⚠️ **"所有技能"不加攻击技门**：本行 tips 字面是"所有技能威力提升"；K290 652
//      「对手处于害怕状态时**攻击**威力翻倍」明写"攻击"才加类型门。本单按字面
//      全部技能都增威，**记档此口径选择**（与 K286 654 同款记档）。
//      ⚠️ 对手已倒（hp≤0）时门闭（K290 652 同款守卫）。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 613 无魂印行为硬编码；
//    ②effect_icon kind=[1,3]、tips 为权威文（dump 原文已核；effect_info 无 613
//    技能模板）；③registry soul 集合无 613、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：diwang_3512 1106（**ws.damage_reduce_add 减伤槽，首个空槽写法，直系**）、
//   k290_fear_power_heal 652（**害怕门 + SKILL_EFFECT 威力改写，直系同路**）、
//   k205_fullhp_power 325 / k165_gate_attach_power 291（view ×(100+pct)/100 赋值形）、
//   k_round_stat 636（SKILL_EFFECT 威力改写时点定版）。
// 场景回归：test/scenario/scenario_4510_reduce_fear_power_613.cpp（号段 4510-4519）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>
#include <entities/skills.h>

namespace {

constexpr int kMarkId = 613;
constexpr int kReducePctFallback = 20;   // tips 字面"减少20%"（db[0]）
constexpr int kGateCodeFallback = static_cast<int>(AbnormalStatusId::Fear);  // 6
constexpr int kAmpPctFallback = 50;       // tips 字面"提升50%"（db[2]）

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

// ① ROUND_START：每回合重写减伤槽（ws 每回合 reset → 必须每回合重写）。
//   4 槽是**并列加算项**（applyDamageReduction 求和），填首个空槽（diwang 同款）。
EffectResult k295_613_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 0, kReducePctFallback);   // db[0] = 20
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// ② SKILL_EFFECT：对手处于 db[1] 异常 → 所有技能威力 ×(100+db[2])/100。
EffectResult k295_613_skill_effect(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：门闭（K290 652 同款守卫）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int gate_code = db_at(row, 1, kGateCodeFallback);   // db[1] = 6（害怕）
    const int pct = db_at(row, 2, kAmpPctFallback);           // db[2] = 50
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core->has_active_anomaly(ctx, opp, gate_code)) {
        return EffectResult::kOk;   // 门闭：对手不处于该异常状态
    }
    auto& view = ctx->ws.skill_power_view[owner];
    if (view.value > 0) {
        view = view.value * (100 + pct) / 100;
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（613 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k295_613_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_FIRST_SKILL_EFFECT, &k295_613_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false),
              SOUL_NODE(State::BATTLE_SECOND_SKILL_EFFECT, &k295_613_skill_effect,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
