#ifndef CHANCE_GATE_LIB_H
#define CHANCE_GATE_LIB_H

// 概率闸门印记族（2026-09-22）——"改概率"的官方印记的**参数预设 + 授予入口**。
// 魂印侧（soul_lib）/ 技能侧（moves_lib）/ 场景测试共用同一组参数，**别各写各的**。
//
// ═══════════════════════════════════════════════════════════════════════════
// 一、这一族是什么
//
// 官方有一批印记，作用不是施加/免疫某个状态，而是**改另一条概率判定的实际值**：
//   · 把 ≤N% 的概率**压到 0%**（亮节）；
//   · 或把 >N% 的**抬到 100%**、≤N% 的压到 0%（英雄之诫——同形，将来同一套票表达）；
//   · 或只做**检测**、不改概率（永沐"检测到 ≤50% 的异常附加就给对手挂曝"）。
//
// 共同点（决定了它只能长成现在这样）：
//   ① 判据是**运行时实际概率**，不是数据表字面值——官方口径「**以实际概率为准**」
//      （战栗被提升到 100% 就不受影响；克罗"首次 40% 会被压、之后超过 50% 不受影响"）。
//      故此处的门必须在**掷骰之前**问，且问的是"这次到底是多少 %"。
//   ② **实时**——亮节"下场不保留"、永沐"位于出战背包"、英雄之诫"下场保留"，
//      生命周期各不同 → 用 RuleCenter 票的 EffectScope（ON_STAGE/TEAM）+ 回合窗口表达，
//      不做任何静态扫描（静态扫描只解决"这条效果属不属于某类"的清点问题，见下）。
//   ③ **与是否触发无关**——梅赫维特原文「不论这个控场是否触发，只要对手打了带有
//      不高于 50% 的控场效果就必定给对手挂曝」→ 处理发生在**裁定**阶段，不是事后事件。
//      这也是 EventCenter（事后 emit、且"概率没过门"根本没有事件）承载不了它的原因。
//
// 二、宿主与文本（官方 effect_icon）
//
//   无极圣武 4800（魂印 effect 2436）：
//     「自身位于出战背包时，双方精灵登场时受到砥砺 1 回合，触发成功则获得 3 回合亮节，
//       未触发则被附加 3 回合威怯」
//   亮节 = kLiangJie：「精灵持有时，套装、特性、魂印、技能中不高于 50% 的附加异常效果
//     全部降低为 0%（**直接针对控场宝石、主动毒、被动毒**）」→ 四类来源全管。
//     挂"亮节"时**给对手**的概率动刀（护的是持有者自己）——故票挂在持有方身上。
//     另有子句「回合结束时给对手 pp 为 0 的技能恢复 5 点」——**属宿主魂印，不在本头**。
//   帝皇侠：魂印印子里**直接复用"圣武亮节"**（语料《机制解析—帝皇侠》原文："火土印记……
//     另外三个印记就是典型的复制代码，**圣武亮节**、月冕和薇的针对场下"）→ 调同一个
//     `grant_liangjie`，**不要另起一套参数**。
//   梅赫维特 4853（魂印 2456）：
//     「永沐：自身位于出战背包时，若对方精灵执行概率不高于 50% 的异常状态附加效果
//       则为对手附加曝」——同为 50% 阈值，但两处不同：**不含特性主动毒/被动毒**
//     （source_mask 去掉 Trait）、后果是**挂"曝"**而非归零（需要在裁定里带回写副作用）。
//     宿主未实现；落地时给本头加 `grant_yongmu` 与 kChanceSourceAll & ~Trait 的掩码。
//
// 三、本头**不做**什么
//
//   · 不写印记的获得链（砥砺触发 / 登场授予 / 回合数记账）——那是宿主魂印的事；
//   · 不写"英雄之耀"的层数清点——那是**静态扫描**（战斗开始时数"己方有几个精灵的特性
//     含 ≤50% 异常附加"），走 `EffectMetaCatalog::is_status_inflict_at_most`，
//     与"本次实际概率多少"是两件事，别混。
//
// ⚠️ 覆盖键：同一来源效果重复授予是**刷新**不是追加（RuleCenter 的覆盖键含
//    source_effect_id + tag）。帝皇侠/无极圣武共用 kLiangJieSourceEffectId → 两者
//    同时在场时**同源不叠加**，与官方"同名魂印的同源效果不叠加"一致。
// ═══════════════════════════════════════════════════════════════════════════

#include <fsm/battleContext.h>

namespace chance_gate {

// ── 来源效果 id（覆盖键的来源维度；亮节没有自己的 effect id，借宿主魂印 id）──
// 无极圣武魂印 effect_icon 2009 / effect_id 2436 就是"亮节"的发放处，帝皇侠复用同一印记。
constexpr int kLiangJieSourceEffectId = 2436;

// ── 亮节的闸门参数（"≤50% → 0"，四类来源全管，只压不抬）──
constexpr int kLiangJieThresholdPct = 50;
constexpr int kLiangJieBelowResult = 0;    // ≤50 → 归零
constexpr int kLiangJieAboveResult = -1;   // >50 → 不改写（亮节只压不抬）

/**
 * 授予亮节：持有方 side 身上挂 rounds 回合的概率闸门（下场不保留 → ON_STAGE）。
 * @param rounds 亮节的剩余回合数（官方"触发成功则获得 3 回合亮节"）
 * @return source_id（提前撤销用 `ctx->revoke_immunity` 同族的 revoke；正常到期由
 *         RuleCenter 的窗口比较自然失效，无需手工撤销）
 */
inline int grant_liangjie(BattleContext* ctx, int side, int rounds) {
    if (!ctx) {
        return 0;
    }
    return ctx->grant_chance_gate(side, kLiangJieSourceEffectId,
                                  ChanceTag::AnomalyAttach, kLiangJieThresholdPct,
                                  kLiangJieBelowResult, kLiangJieAboveResult,
                                  kChanceSourceAll, rounds, EffectScope::ON_STAGE);
}

/** 撤销亮节（按授予句柄）。 */
inline void revoke(BattleContext* ctx, int source_id) {
    if (ctx && source_id > 0) {
        ctx->rule_center_.revoke(source_id);
    }
}

}  // namespace chance_gate

#endif  // CHANCE_GATE_LIB_H
