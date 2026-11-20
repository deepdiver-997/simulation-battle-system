// J86 · 状态门伤害/回血族 —— 552×19 / 194×19（2026-10-03，worktree ../agents/j86）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 552（args_num=2）"若对手处于异常状态，则{0}%概率附加{1}点伤害"：
//   - 门 = 对手任一异常（has_active_abnormal_status(target)，J14 助手判据）；
//   - 掷 = roll_percent({0})（93 概率固伤同通道）→ fixed_damage(target, {1})
//     （SKILL_EFFECT 直通——29/683 固伤先例时点）。
//   - 实例：破冰狂击怒 19684 [552]|[100,300]（特150 纯链 100% 档）。
// 194（args_num=3）"造成伤害的1/{0}恢复自身体力，若对手{1}，则造成伤害的1/{2}恢复
//   自身体力"：
//   - 裸伤台账口径（422/101/628 族：挡伤/锁伤改变不了它）——需内核 effect_register_state
//     case 194 归位 BATTLE_FIRST_AFTER_ACTION 一行（101/422/628 同款）；
//   - 条件切换读法：基础回 raw/{0}；对手处于异常 {1} → 改回 raw/{2}（"则"=条件替换；
//     实载 1/{2} 恒大于 1/{0}，如 [6,5,3]：冻伤时 1/6→1/3 升档）；
//   - {1} = 异常码单码门（has_active_abnormal_status(target, code)，J63 133 同款）。
//   - 实例：寒光冰魄 13177 [194]|[6,5,3]（特150 纯链）。
//
// 动工前置三查（2026-10-03）：①内核 grep 552/194 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   552=2/194=3 与 effect_info.args_num 一致；38 载体切片自洽。
// 内核改动：skills.cpp effect_register_state 加 case 194 一行（与并行任务错峰）。
// 先例：J14/J63（状态门）、93（概率固伤）、J54 628（AFTER_ACTION 台账回血归位行）、
//   bailuo 2440（raw×%回血）。
// 场景回归：test/scenario/scenario_1735_status_gate_damage.cpp（号段 1730-1739 尾段
//   1735 起——1730 已被 J83 场景占用，同段错开）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 对手任一异常（has_active_anomaly 走 CoreApi 槽——BattleContext 成员非 inline
// 插件不可直调；J14 助手判据，匿名空间不跨 TU，就地重写）。
bool j86_has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= 43; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

}  // namespace

// ── 552 —— 若对手处于异常状态，则{0}%概率附加{1}点伤害 ──
// int_args = [owner, target, chance, fixed]（args_num=2）。
EffectResult effect_552_anomaly_gate_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 552, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 552, args,
                     "552 布局=[owner,target,概率,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 552, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && fixed > 0, 552, args, "需要 概率/固伤 > 0");
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    if (!j86_has_any_anomaly(ctx, target)) {
        return EffectResult::kOk;   // 门：对手处于异常状态
    }
    if (roll_percent(chance)) {
        (void)g_core->fixed_damage(ctx, target, fixed);
    }
    return EffectResult::kOk;
}

// ── 194 —— 造成伤害的1/{0}回血；对手处于异常{1}则改回 1/{2} ──
// 注册时点 = AFTER_ACTION（内核 case 194 归位行）；台账=本次攻击裸伤。
// int_args = [owner, target, denom, anomaly, denom_up]（args_num=3）。
EffectResult effect_194_cond_lifesteal_upgrade(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 194, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 194, args,
                     "194 布局=[owner,target,denom,anomaly,denom_up]");
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom_up = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && target >= 0 && target <= 1, 194, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && denom_up > 0, 194, args, "需要 分母 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 194, args, "异常码越界（≤43）");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int dealt = ctx->ws.raw_attack_damage[self];
    if (dealt <= 0) {
        return EffectResult::kOk;
    }
    const int use_denom =
        (g_core->has_active_anomaly && g_core->has_active_anomaly(ctx, target, anomaly))
            ? denom_up : denom;
    const int heal = dealt / use_denom;
    if (heal > 0) {
        (void)g_core->heal_amount(ctx, self, heal);
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(552, effect_552_anomaly_gate_fixed)
SKILL_EFFECT(194, effect_194_cond_lifesteal_upgrade)
