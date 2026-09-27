#ifndef BURN_PERCEPTION_H
#define BURN_PERCEPTION_H

// ═══════════════════════════════════════════════════════════════════════════
// 烧伤感知层 ——「处于烧伤状态」的统一查询（真实烧伤 ∪ 虚拟烧伤）
//
// 背景（2026-09-20 烧伤三精灵线）：永烬劫炎·上古炎兽 4875 魂印第二/三子句
//   「自身存活于出战背包时，对手免疫能力下降状态时**视为处于烧伤状态**」——
// "视为烧伤"是**感知层**概念，不是异常槽里的真异常：
//
// 官方依据（语料 `reference/scrape/data/detail/1171177636822515734.json`，
//   新萌小山东《【赛尔号】机制解析—永烬劫炎·上古炎兽》，文集「精灵魂印判定细节」）：
//   ① 「实时结算，对手免弱期间内都会视为处于烧伤状态」→ 查询式判定（免弱票过期即失效），
//      不是一次性的标记位。本层实现即 `对面背包有活着的 4875 && 本方在场宠免弱票命中`。
//   ② 「视为烧伤状态**可以响应**一些"处于烧伤状态则xxx"之类的效果，但是
//      "**处于异常状态**则xxx"就不会响应」→ 按**描述措辞**分流（见下方三类）。
//   ③ 「"视为处于烧伤"的精灵会**扣 1/8 真伤**和**威力减半**，同时会给炎魔印记
//      （= 神惧劫炎），不论免弱精灵是否处于烧伤状态」→ 虚拟烧伤带**伴生效果**，
//      与真实烧伤在"1/8 真伤 + 攻击技能威力减半"上行为一致（落点见 §四类）。
//   ④ 「这个"攻击处于烧伤的对手，令对手焚烬"（effect 572），这个效果在**解控之后**，
//      所以正常手段是拿不到的，但是**配合魂印的视为处于烧伤就可以触发**」
//      → 572 这类**条件式**"若处于烧伤则…"**看虚拟**；而"烧伤→焚烬"的**状态转化**
//      （影中艳「将对手所处的烧伤转化为焚烬」）要真异常（槽里得先有烧伤可转）。
//
// ── 用法速查（三类，别混）────────────────────────────────
//   §1 检测类「处于烧伤状态则…」      → is_burn_effective（真实 ∪ 虚拟）
//   §2 条件类「若对手处于烧伤则…」    → is_burn_all（同上；语义与 §1 相同，别名便于检索）
//   §3 状态转化「将对手所处的烧伤转为…」→ is_real_burn（**只认真异常**——槽里得有烧伤）
//   §4 伴生效果「1/8 真伤 + 威力减半」→ 引擎侧已接入（battleFsm）：
//        · 威力减半：`handle_BattleFirst/SecondActionStart` 的威力物化处查 is_burn_effective；
//        · 1/8 真伤：`stage_action_start_abnormal_damage` 在无真实烧伤时补一段虚拟烧伤段。
//
// header-only：soul_lib / moves_lib / core / 场景测试共用；只读 BattleContext 公有成员，
// 不链接任何符号（同 curse_stack.h 纪律）。
// ═══════════════════════════════════════════════════════════════════════════

#include <abnormal-system/abnormal-types.h>
#include <fsm/battleContext.h>

// 虚拟烧伤的**来源**精灵：永烬劫炎·上古炎兽。
constexpr int kVirtualBurnSourcePetId = 4875;

// 真实烧伤：异常槽里的烧伤(2)且未到期（end 为 exclusive，roundCount < end 生效）。
inline bool is_real_burn(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    return ctx->roundCount
        < ctx->abnormal_status_end_round[side][static_cast<int>(AbnormalStatusId::Burn)];
}

// 「自身存活于出战背包」：对面 6 格背包里有活着的上古炎兽（在场/场下都算；
// 体力上限归零 = 已消逝，剔除）。
inline bool opposing_party_has_alive_yanshou(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    const int opp = 1 - side;
    for (const ElfPet& pet : ctx->seerRobot[opp].elfPets) {
        if (pet.id == kVirtualBurnSourcePetId && pet.hp > 0
            && pet.numericalBase[NumericalPropertyIndex::HP] > 0) {
            return true;
        }
    }
    return false;
}

// 虚拟烧伤：side 一侧在场宠**免疫能力下降**且对面有活着的上古炎兽。
// 免弱是查询式判定（is_immune 纯查询）：免弱票覆盖当下时点才算，过期/换宠清了即不成立
// ——与官方"实时结算，对手免弱期间内都会视为处于烧伤"一致，无需维护标志位。
inline bool has_virtual_burn(const BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return false;
    }
    if (!opposing_party_has_alive_yanshou(ctx, side)) {
        return false;
    }
    const int slot = ctx->on_stage[side];
    if (slot < 0 || slot > 5) {
        return false;
    }
    return ctx->is_immune(side, ImmunityType::STAT_DROP, ctx->currentState);
}

// §1/§2 —— 统一烧检 = 真实 ∪ 虚拟。**检测类 / 条件类**效果的唯一入口
// （"处于烧伤状态则…" / "若对手处于烧伤则…"）。
inline bool is_burn_effective(const BattleContext* ctx, int side) {
    return is_real_burn(ctx, side) || has_virtual_burn(ctx, side);
}

// §2 的别名（语义与 is_burn_effective 完全一致，只是让"条件式"调用点在检索时更显眼）。
inline bool is_burn_all(const BattleContext* ctx, int side) {
    return is_burn_effective(ctx, side);
}

#endif // BURN_PERCEPTION_H
