// J 组 J137 · DETECT 回吸族 —— 3 宏/35 载体（2026-10-04，worktree ../agents/j137）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   1415 [pct,pct_up]  "造成伤害的{0}%恢复自身体力，若自身体力低于对手则效果提升至{1}%"
//     —— DETECT **攻方桶**读 final（989/J15-1088 直系位）：红伤 final>0 →
//     heal_amount(自身, final×pct/100)；hp 对比在 DETECT 时点求值（红伤未扣血，
//     工单口径）——self.hp < opp.hp → 用 {1}，否则 {0}（"提升至"=取大档，两参即档）。
//   687  [异常码,pct]  "若对手{0}，则对对方造成伤害的{1}%恢复自身体力"
//     —— 同桶 + has_active_abnormal_status(防御方,{0}) 门 → heal final×{1}%。
//     门在 DETECT 时点判（当刀结算时对手状态）。
//   1675 [rounds]      "反转对手能力提升状态，反转成功则对手{0}回合内无法通过自身
//     技能恢复体力" —— 反转门（J19 978 直系：快照→stat_boost_reversal→REVERSED
//     才走后缀；失败无事发生）→ grant_immunity(对手, HEAL_BLOCK, duration={0})
//     （J17 1247 票形直系；票=绝对窗 register_round 起算 {0} 回合，含当回合）。
//     回血走 heal_amount 原语天然吃封回血（嗑药不吃的引擎口径不动）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 12+11+12=35 全载体自洽、零剩余）：
//   1415 [pct,pct_up] ×12：[50,100]×4、[40,80]×2、[50,80]、[25,50]、[30,50]、
//     [50,75]、[40,60]、[75,150]——攻击技载体（cat1/cat2）
//     纯链：恶魔汲血刃 33161 [50,100]（物160）
//   687  [异常码,pct] ×11：[2,100]×4（烧伤）、[2,40]×2、[20,50]×2、[2,50]、[8,50]、
//     [6,100]（害怕）；纯链：熔岩裂地 16791 [2,100]、蔽夜魂羽 36373 [6,100]
//   1675 [rounds] ×12：[2]×11、[3]×1；纯链：寻息追踪 34915 [2]（物85）
//
// 动工前置三查（2026-10-04）：①内核 grep 3 id 无硬编码 ②custom_effect_overrides
//   零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J42 989/J15 1088（DETECT 攻方桶）、J19 978（反转成功门）、J17 1247/797/1361
//   （HEAL_BLOCK 票形）、heal_amount 吃封回血（core_api 口径）。
// 场景回归：test/scenario/scenario_2320_detect_drain3.cpp（号段 2320-2329，
//   三纯链载体 + raw_attack_damage 精确差账）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// DETECT 攻方桶公共体：红伤 final>0 → 按分母恢复自身（heal_amount 原语）。
void j137_heal_by_final(BattleContext* c, int owner, int pct) {
    if (pct <= 0 || !g_core || !g_core->heal_amount) {
        return;
    }
    const DamageSnapshot& d = c->resolvedDamage;
    (void)g_core->heal_amount(c, owner, d.final * pct / 100);
}

// ── 1415 —— 造成伤害的{0}%回体；自身 hp<对手 → 提升至{1}% ──
// int_args = [owner, target, {0}=pct, {1}=pct_up]（args_num=2）。
EffectResult effect_1415_damage_heal_low_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1415, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1415, args,
                     "1415 布局=[owner,target,pct,pct_up]");
    const int owner = args.int_args[0];
    const int pct = args.int_args[2];
    const int pct_up = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 1415, args, "owner 越界");
    EFFECT_ARG_GUARD(pct > 0 && pct_up >= pct, 1415, args, "需要 pct>0 且 pct_up>=pct（提升至）");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, pct, pct_up](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final <= 0) {
                return;   // 有红伤落地才回
            }
            // hp 对比在 DETECT 时点（红伤未扣血，工单口径）。
            const bool low = c->getPet(owner).hp < c->getPet(d.defenderId).hp;
            j137_heal_by_final(c, owner, low ? pct_up : pct);
        });
    return EffectResult::kOk;
}

// ── 687 —— 若对手{0}状态，则造成伤害的{1}%回体 ──
// int_args = [owner, target, {0}=异常码, {1}=pct]（args_num=2）。
EffectResult effect_687_anomaly_gate_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 687, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 687, args,
                     "687 布局=[owner,target,异常码,pct]");
    const int owner = args.int_args[0];
    const int anomaly = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 687, args, "owner 越界");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 687, args, "异常码越界（≤43）");
    EFFECT_ARG_GUARD(pct > 0, 687, args, "需要 pct > 0");
    ctx->register_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, anomaly, pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            const DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || d.defenderId < 0 || d.defenderId > 1) {
                return;
            }
            if (!d.isRed || d.final <= 0) {
                return;
            }
            if (!g_core->has_active_anomaly(c, d.defenderId, anomaly)) {
                return;   // 门：对手处于{0}状态（当刀结算时判；CoreApi 495 同口）
            }
            j137_heal_by_final(c, owner, pct);
        });
    return EffectResult::kOk;
}

// ── 1675 —— 反转成功 → 对手{0}回合 HEAL_BLOCK ──
// int_args = [owner, target, {0}=回合数]（args_num=1）。
EffectResult effect_1675_reverse_heal_block(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1675, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1675, args,
                     "1675 布局=[owner,target,回合数]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && opp != owner, 1675,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1675, args, "需要 回合数 > 0");
    if (!g_core || !g_core->stat_boost_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, opp) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 反转失败（免弱挡下/无提升可反）→ 无后缀（978 记档）
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1675,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1675);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1415, effect_1415_damage_heal_low_up);
SKILL_EFFECT(687, effect_687_anomaly_gate_heal);
SKILL_EFFECT(1675, effect_1675_reverse_heal_block);
