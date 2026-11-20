// K275 免疫所有异常族 —— effect 644（1 宏 / 1 行）
//
// 行清单（tools/soul_effects_dump.py 644，2026-10-21）：
//   644 谱尼 300 args[1,0]（kind=[7]）：「免疫所有异常状态（boss免疫)」
//     → **args 无语义不消费**（记档）：db[0]=1 / db[1]=0 在本行找不到对应文本子句；
//       "免疫所有异常"是**全量**语义（不带百分比/条件），按 tips 全异常定值。
//       工单行记「参数零语义按 tips 全异常定值」——与之一致。
//
// ⚠️ 实现形态（T4 类型化行参 + db_at 原序读行参）：
//   ⚠️ **必须走 db_at**（读宿主 pet.soulMark.args.owned_int_args 原序），不能走
//      a.row（类型化 SoulNodeArgs）——后者在**真实对局里是空的**（bind_soulmark_args
//      丢弃 owned_int_args 前两位，而真实对局的 owned_int_args 就是 DB args 原样，
//      通常 2~8 位）→ a.row = owned[2:] 常常为空。存量 17 个用 a.row_or 的文件全靠
//      **fallback 硬编码**兜底，测试能过是因为场景按 {0,0,...行参} 构造（多带两位
//      占位）。本单实现与场景**两边都对**（K257 起的夜跑口径）。
//
//   效果 = grant_immunity(ANOMALY, coverage_all, anomaly_mask=0 全异常, **永久票**)
//     · anomaly_mask=0 → RuleCenter 语义是"全异常免疫"（rule_center.h:553：mask≠0
//       才按位细分；mask=0 时不设限）——正合"免疫所有异常状态"字面。
//     · duration_rounds=**0** = 永久票（rule_center.h window_expired：remaining_rounds
//       >0 才判过期，0 = 不过期）。⚠️ **不需要每回合重授**——工单行记的"ws 每回合
//       reset → ROUND_START 重写"对本单**不成立**：免疫票存于 RuleCenter（rule_center_），
//       **不在 ws 里**，ws.reset() 不碰它（只下场时 clear_on_stage 清）。用永久票 +
//       固定句柄，同 source_id 重复授予是**刷新**不是追加（rule_center.h grant_immune
//       ①路径），故 enter 首挂一次即可覆盖整场。
//     · EffectScope::ON_STAGE → 换宠即清（免疫挂被护方自身 source==target，
//       invalidate_on_stage_effects 里 rule_center_.clear_on_stage 清的是下场槽）——
//       魂印随宿主走，符合"魂印效果随精灵上下场"口径。
//     · source_effect_id=644 → 覆盖键的来源维度（与同 type 其他来源各占一条）。
//
//   ⚠️ **Ancient 档**（ ImmunityTier::Ancient）——工单行记"Ancient 层全 mask 票"，
//     与 diwang_3512（帝皇之御，"2016 老精灵按'老魂免'档"）同款。档位语义（官方实测
//     定档，见 curse_stack.h 头注）：Modern 档**只挡现代施加通道**，古早施加通道
//     （主动毒等 apply_anomaly_ancient）**只查 Ancient 层**——老魂印按"老魂免"档走
//     Ancient，故能挡主动毒。谱尼（2013 老精灵）同档。
//
//   ⚠️ **（boss免疫）尾注口径 = boss 有效**（2026-10-21 三方独立取证，与 K257 的
//     相反解读并存，**记档待白班统一**）：
//     ① 官方语料《机制解析—战龙哈莫》(bilibili opus/784274231187734528) 把
//        "（boss免疫）"尾注用在**主角自己的魂免效果**上：「每回合开始时若自身体力
//        大于1/2最大HP则免疫异常状态（boss免疫）」——是"我的免控对 boss 也生效"，
//        不是"对 boss 无效"。同一篇正文详述该魂免的 boss 对局判定。
//     ② resources/boss_invalid_effects.json（1397 个 id，只收 tips 含"boss无效"的
//        行）**不含 644**——"boss免疫"是独立措辞，不在机械清单内。
//     ③ 工单行由立单方查证后写明"boss 有效"。
//     → 本单节点 boss_invalid=**false**（boss 挑战对局也注册执行）。
//     ⚠️ 对照：K257（357 同款"（boss免疫）"尾注）用了 boss_invalid=**true**，与
//        上述三证相反。357/360/738 同属"boss免疫"4 行，均未在 boss_invalid 清单内。
//        **口径分歧留档**——若白班裁定"boss免疫 = boss 无效"，本单与 K257 一并改。
//
// ⚠️ 三查记录（author a02-kprod 2026-10-21）：①内核 grep 644 无魂印行为硬编码；
//    ②effect_icon kind=[7]、tips 为权威文（dump 原文已核，4399 语料 soul_seal.text
//    交叉验证一致：「圣：免疫所有异常状态（boss免疫)」；effect_info 无 644 技能模板）；
//    ③registry soul 集合无 644、soul_lib 全库 grep 无同 id 注册——真缺口。
// 先例：diwang_3512（帝皇之御 1106：Ancient 层全 mask 免异常票，权威同款档位）、
//   k_fullhp_immune_family 285（满体力免异常：enter + ROUND_START 双节点 + 固定句柄
//   + duration=1 每回合重授）、artemis_4745（**永久票** duration_rounds=0 + 条件回调，
//   证明永久票可行）、k_open_window_immune 221（魂免 + soul_immunity=true）。
// 场景回归：test/scenario/scenario_4310_immune_all_anomaly_644.cpp（号段 4310-4319）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kMarkId = 644;
constexpr int kImmuneHandle = 64401;   // 固定句柄：同 source_id 重复授予 = 刷新非追加

// ENTER_EXIT_STAGE：首挂**永久**全异常免疫票（Ancient 层）。
EffectResult k275_644_grant(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    // ⚠️ 定位宿主槽：免疫票挂**被护方自身**（source==target==owner），ON_STAGE 域
    //   随下场清——故必须确认宿主在场上（onstage_has_soul_mark 已判）。
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/0,        // 0 = 全异常免疫（不限码）
                        /*duration_rounds=*/0,     // 0 = 永久票（不过期）
                        /*source_id=*/kImmuneHandle,
                        /*soul_immunity=*/true,    // 魂免标记（K163/221 同款）
                        EffectScope::ON_STAGE,
                        /*counts=*/0,
                        /*source_effect_id=*/kMarkId,
                        ImmunityTier::Ancient);    // 老魂免档：古早通道（主动毒）也挡
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（644 入账，只增不改）──────
// ⚠️ boss_invalid=**false**（"（boss免疫）" = boss 有效，见头注三证）。
SOUL_PROGRAM(kMarkId,
             {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k275_644_grant,
                        /*once=*/false, /*early=*/false, SoulScope::STAGE,
                        /*boss_invalid=*/false)});
