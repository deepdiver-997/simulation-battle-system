// K311 烧伤门属性技失效族 —— effect 323（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 323，2026-10-21）：
//   323 神炎 2666 args[2,2,4]（kind=[13]）：「若对手处于烧伤状态，则对手释放的所有
//     属性技能失效」
//     → **db[0]=2 = 烧伤码**（官方码 Burn=2；本单据此消费）——但**位置存疑，记档**：
//       同族行的烧伤码多在**末尾位**（114 args[10,2] / 265 args[1000,6,2] /
//       318 args[1,20,2] / 358 args[1,1,30,2]），本行 db[0]=2 在**首位**。
//       ⚠️ 故本单**不消费 db[0]**，异常码按 **tips 字面**（"烧伤"）取官方码
//         Burn=2 硬编码——**tips 是权威文**（SOP §六.1），参数位不可解时以文为准
//         （K2 纪律「解析不了宁可不用、不猜」）。
//       db[1]=2 / db[2]=4 语义存疑不消费（记档）：本行无第二个数值子句可锚定
//       （"所有属性技能失效"是全量语义，不带次数/回合数）。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   本单**不读任何行参**（按 tips 字面定值）——与 K304 1738 / K309 802 零参先例同款。
//   ⚠️ 走 db_at 而非 a.row 的理由同族统一（K257 起的夜跑口径）：a.row 在**真实对局**
//      里是空的（bind_soulmark_args 丢弃 owned_int_args 前两位）→ 存量用 a.row_or 的
//      文件全靠 fallback 硬编码兜底。本单不消费下标，故两种读法等价。
//
//   门 = 对手处于**烧伤**状态（官方码 Burn=2）。
//   效果 = seal_skill(source=印主, target=对手, attribute=true, attack=false,
//     count=0（不限次数）, duration_rounds=1（**每回合窗**）) —— **属性技封属**。
//     ⚠️ **为什么每回合重授**：官方"若对手处于烧伤状态，则对手释放的所有属性技能失效"
//        是**条件持续**语义——对手一直烧伤就一直失效，没有回合数上限。故不能挂一次
//        永久票（烧伤解除后就不该再封）；改为**每回合 ROUND_START 重授 duration=1 的
//        封属窗**（K171 285「满体力免异常」同款"每回合重挂窗票"套路；
//        k_hit_suppress_cond / k228 / k251 的"单次授予 duration=N，靠 tick 自灭"同源）。
//        烧伤解除后的下一回合不再重授 → 自然恢复。
//     ⚠️ **"失效"而非"命中效果失效"**：走 SEAL_ATTRIBUTE（**非** hit_invalid）——
//        chaos_sorensen 1011 头注明写：官方此处是"无效"→ 属性技能走 ②被无效，触发
//        技能无效补偿（1217"消强成功则对手下N次攻击技能无效"、936"未触发则封属性"
//        同族先例）。
//     ⚠️ count=0 = **不限次数**（官方"所有属性技能"）；duration_rounds=1 = 每回合窗。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 323 无魂印行为硬编码；
//    ②effect_icon kind=[13]、tips 为权威文（dump 原文已核；effect_info 无 323
//    技能模板）；③registry soul 集合无 323、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：chaos_sorensen 1011（**SEAL_ATTRIBUTE 属性封属 + 技能无效补偿口径，直系**）、
//   encarlos_light_3577 1164（封属 + 命中前桶插票）、k_fullhp_immune_family 285
//   （**每回合重挂窗票**套路）、k_hit_suppress_cond（封属 duration 绝对窗）。
// 场景回归：test/scenario/scenario_4670_burn_seal_attr_323.cpp（号段 4670-4679）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 323;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2（tips 字面，硬编码）
constexpr int kSealHandle = 32301;   // 固定句柄：同 source_id 重复授予 = 刷新非追加

// ROUND_START：对手处于烧伤 → 挂属性技封属窗（duration=1，每回合重授）。
EffectResult k311_323_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->seal_skill || !g_core->has_active_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    const int opp = ctx->opponent(owner);
    if (!g_core->has_active_anomaly(ctx, opp, kBurn)) {
        return EffectResult::kOk;   // 门闭：对手不处于烧伤状态
    }
    // 属性技封属：count=0（不限次数，"所有属性技能"）、duration_rounds=1（每回合窗）。
    // ⚠️ 固定 source_id → 同句柄重复授予是**刷新**不是追加（RuleCenter::grant_seal
    //    覆盖键刷新语义）——故每回合重授不会叠加成多张票。
    (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp,
                             /*effect_id=*/kSealHandle,
                             /*attribute=*/true, /*attack=*/false,
                             /*count=*/0, /*duration_rounds=*/1,
                             /*penetrable=*/true, /*source_slot=*/-1,
                             EffectScope::ON_STAGE, /*hit_invalid=*/false,
                             /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（323 入账，只增不改）──────
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::BATTLE_ROUND_START, &k311_323_round_start,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
