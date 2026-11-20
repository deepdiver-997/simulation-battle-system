// G 组批产 · G4 —— 异常/降级条件族：745×29 / 167×28（2026-09-29，worktree ../agents/g4）
//
// 家族语义：两条都是"条件判据 + 常规通道落地"——降级走 stat_drop 正规通道
// （免弱 STAT_DROP 检查在原语内收口，1191/1990 同款）、附加伤害走 deal_pink_damage
// (FIXED)（插件造粉伤唯一入口，1244/1214 同款）。不直写本体/视图/hp。
//
// 745（args_num=6）"使对手{0}，对手处于异常状态时弱化效果翻倍"：
//   - 载体：火舞枫叶 17361（特攻 85 先制+2，[745]/[0,0,0,-1,0,-1]）、
//     炼狱环境 23934（属性技 0，[745,48]/[-1,0,-1,0,0,-1,3]——链尾 [3] 是
//     48 的"3回合免疫异常"，48 已在 anomaly_channel.cpp 实现，不归本件）。
//   - {0} = **六槽降级表，官方语义序**（攻,防,特攻,特防,速,命中）→ 引擎等级槽
//     {0,2,1,3,4,5}（stat_dispel.cpp kStatMap / 477 同款转换）。4399 双载体交叉
//     验证：火舞枫叶 [0,0,0,-1,0,-1]="使对手**特防**-1、命中-1"、炼狱环境
//     [-1,0,-1,0,0,-1]="使对手**双攻**-1、命中-1"——语义序 idx2=特攻/idx1=防
//     与引擎序正好互换，炼狱环境这条把两位钉死（按引擎序直读会把"双攻"读成
//     "攻+防"）。
//   - 翻倍判据 = 对手处于**任意**异常：遍历官方异常码 0..43 查
//     g_core->has_active_anomaly（F3 1214 同款，自带"视为不处于异常"虚拟视图）。
//   - ⚠️ 时序记档：文本"施加时若对手有异常则翻倍"= **施加动作内判**——handler
//     入口先判对手当前异常、再逐槽施加（先判后加）。降级本身不产生异常，两种
//     顺序在此判据下等价，按字面取"判据看施加时刻的对手状态"。
//   - 幅度 = |v| ×（对手有异常 ? 2 : 1）逐槽 stat_drop（1990 全属性降翻倍同款
//     通道；载体观测值全为 0/-1/-2，非零一律按降级幅处理）。
// 167（args_num=1）"若对手处于能力下降状态则附加{0}点伤害"：
//   - 载体：深海狂啮 12887（物攻 150，[167]/[100]）、残影叶刃闪 13423
//     （特攻 150，[167]/[30]）。全库 28 载体，{0} ∈ 30..300。
//   - 判据 = 对手六槽能力等级（0攻..5命中）任一 **< 0**（F8 1244 同款，权威态
//     ctx->ability_levels 现读——命中结算点 SKILL_EFFECT 桶时若链上更早的效果
//     刚降了对手等级，本条同回合可见，"能力下降状态"按判定时刻的实况）。
//   - ⚠️ 时序记档：文本"若对手处于能力下降**则附加**"= **先判后加**——判据不
//     成立就不造这笔粉伤（对照 1244 是"两档区间"、745 是"幅度翻倍"，本条是
//     0/全有 gates）。
//   - 落点 = deal_pink_damage(FIXED, {0})（粉在红后是 SKILL_EFFECT 桶既有时序，
//     60/1244 同桶先例）。
//
// 前置三查（2026-09-29）：①内核无 745/167 硬编码（grep src/ include/ 无命中）
// ②custom_effect_overrides 无 745/167 行（48 有行但已由 anomaly_channel.cpp
// 实现，链尾不缺）③moves_lib 无既有注册（SKILL_EFFECT(745/167) 零命中）。
//
// 场景回归：test/scenario/scenario_740_cond_debuff.cpp（740-749 号段，一文件覆盖两效果）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 官方语义序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
// stat_dispel.cpp kStatMap={0,2,1,3,4,5} 同款（1/2 两位互换——防/特攻读反即错）。
constexpr int kSemanticToEngineSlot[6] = {0, 2, 1, 3, 4, 5};

// 对手是否处于任意异常（F3 1214 同款遍历 0..43；has_active_anomaly 缺席时按"无异常"）。
bool foe_has_any_anomaly(BattleContext* ctx, int opp) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, opp, id)) {
            return true;
        }
    }
    return false;
}

// ── 745 —— 使对手{0}（六槽降级表·语义序），对手处于异常状态时弱化效果翻倍 ──
// int_args = [owner, target, 攻, 防, 特攻, 特防, 速, 命中]（降级幅度，负值）。
EffectResult effect_745_conditional_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 745, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 745, args,
                     "需要 [owner,target,六槽降级表(语义序)]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     745, args, "需要 owner/target ∈ {0,1}");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    // 施加动作内判（头注时序记档）：对手有任意异常 → 全表幅度 ×2。
    const int scale = foe_has_any_anomaly(ctx, opp) ? 2 : 1;
    for (int i = 0; i < 6; ++i) {
        const int v = args.int_args[2 + i];
        if (v == 0) {
            continue;   // 0 = 该槽不动（火舞枫叶只动特防/命中两槽）
        }
        const int magnitude = (v < 0 ? -v : v) * scale;
        (void)g_core->stat_drop(ctx, opp, kSemanticToEngineSlot[i], magnitude, owner);
    }
    return EffectResult::kOk;
}

// ── 167 —— 若对手处于能力下降状态则附加{0}点伤害 ──
// int_args = [owner, target, 附加点数]。
EffectResult effect_167_bonus_fixed_vs_dropped(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 167, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 167, args,
                     "需要 [owner,target,附加点数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner,
                     167, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(amount > 0, 167, args, "需要 附加点数 > 0");
    if (!g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    // 先判后加（头注时序记档）：对手六槽（0攻..5命中）任一负等级（1244 同判据）。
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] < 0) {
            g_core->deal_pink_damage(ctx, opp, amount, DamageKind::FIXED, owner);
            return EffectResult::kOk;
        }
    }
    return EffectResult::kOk;   // 对手无任何下降 → 不附加
}

}  // namespace

SKILL_EFFECT(745, effect_745_conditional_drop)
SKILL_EFFECT(167, effect_167_bonus_fixed_vs_dropped)
