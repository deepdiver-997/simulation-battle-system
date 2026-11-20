// J254 · 睡眠门暴击率族 —— 95×5（2026-10-24，worktree ../agents/j254）
//
// 家族语义（effect_info 模板 + 4399 载体文本交叉定标）：
//
// 95（args_num=1）"对手处于睡眠状态时，致命一击率提升{0}%"：
//   - 模板尾巴的 "%" 是官方松口径——**{0} 的真实单位是 1/16**（十六分之几），与内核
//     effect 32 蓄气族"+1/16 = +6.25 个百分点"（battleWorkspace.h crit_rate_add 注释）
//     同一单位族。4399 四载体全部吻合：
//       多彩光波 11494 [4] → "致命一击率提升25%"（4/16）
//       蚀梦术   12794 [4] → "致命率提高25%"（4/16）
//       美梦暴击 36987 [4] → "致命一击率提升25%"（4/16）
//       花粉风暴 16307 [8] → "致命一击率提升1/2"（8/16 = 50%——4399 直接写最简分数）
//     （若按 1/{0} 分母读，花粉风暴 8→12.5% 与官方 1/2 直接矛盾 → 分母读法排除。）
//   - 条件 = **对手（守方）处于睡眠(8)**；语义 = 本次攻击的暴击率加成（攻击打睡眠目标
//     才享受——"美梦暴击"的字面）。官方时点口径（K2"命中即醒"）：睡眠在 ON_SKILL_HIT
//     的 anomaly_on_attack_hit_hook 里被本次攻击清除，但暴击掷点（query_usage ①.5）
//     在那**之前**——掷骰时目标仍是睡眠态，加成成立，两不相扰。
//
// 实现口径：
//   - **选择期路由**（内核 loadSkills selection_effects_ 表 +95，2026-10-24）：暴击掷点
//     在 Skills::execute 内 query_usage ①.5——SKILL_EFFECT/ON_SKILL_HIT 桶全在它之后
//     （J289 168 的威力写赶的是 ATTACK_DAMAGE 伤害管线，不是掷骰），插件从任何"用技后"
//     桶写 crit_rate_add 都吃不到本次攻击。选择期节点在本回合 MOVE_RIGHT 桶触发
//     （先制比较/resolve/掷骰之前），条件当场评估、直写 ws.crit_rate_add[owner]。
//   - crit_rate_add 是**加算百分点**通道（rate = base×mod + add；add>0 兼"引爆"：
//     载体 crit_rate=0 照样能暴）——每回合 NSDMI reset 归零，本实现每回合被选中时
//     重写，无跨回合泄漏（J146 441 同通道）。
//   - 评估时点精确性：异常自然结算在回合扣减点（tick_abnormal_statuses 挂
//     ROUND_REDUCTION 档，battleFsm:2152）——回合内"ROUND_START 已睡"的目标整回合都睡
//     （1v1 中唯一会打醒它的是我自己的攻击，掷骰在其后）→ MOVE_RIGHT 快照即掷骰时点
//     真值。记档边角：①若本回合**先**被（额外行动等）中途铺睡，MOVE_RIGHT 已过 →
//     当回合吃不到加成（自身同链铺睡本就在掷骰后，无影响）；②被控/被封时节点照写
//     （J74 747 同款"被控也生效"），若同回合经额外行动换了别的技能出手，残留加成会
//     被吃到——选择期武装的家族共性，量级可忽略。
//
// plugin_storage 审计槽（grep 全库无撞）：
//   990095000 + owner = 本次申报加成（百分点 ×100 整化：4→2500、8→5000；条件不成立=0；
//   未选中该回合=键缺席）——概率效果无随机精确断言口径（F9 782 / J146 441 同款）。
//
// 动工前置三查（2026-10-24，worktree ../agents/j254）：
//   ①内核 grep 95 无行为硬编码（"睡眠解除"钩子是通用异常语义、crit_rate_add 是通用
//     加算钩子，均非专设；battleContext.h:400 "信仰对象 41" 是异常 id 撞号空间的另一码事）
//   ②effect_info 95 模板唯一（args_num=1）、custom_effect_programs/overrides 零行、
//     effect_icon 无行
//   ③moves_lib/soul_lib 全库并集无 SKILL_EFFECT(95) 注册、parser 两刀均不接（模板无
//     "令/使…{状态}"主句、非 179/700/1083/1257/456 五类）——真缺口。
//   载体切片（side_effect.arg_count 优先，95 arg_count=1，脚本核算）：
//     纯链×4：11494 [95]|[4] / 12794 [95]|[4] / 16307 [95]|[8] / 36987 [95]|[4]
//     混链×1：寸阴是竞 33131 [868,95]|[2,4]（868 已实现、args[0]=2 归 868，自洽）
//   真实精灵覆盖 21 只。
// 内核改动：skills.cpp loadSkills selection_effects_ 路由表 +95 一项（2298/747/1956 同款；
//   与并行任务错峰动 skills.cpp——本次仅追加一行 || 与注释块）。
// 先例：J74 747（selection 路由形 + [owner,target,arg] 布线）、J289 168（睡眠判据
//   g_core->has_active_anomaly(opp,8) + ON_SKILL_HIT 时点勘误）、J146 441（crit_rate_add
//   写入通道 + 审计槽）、effect 32（+1/16 单位族）。
// 场景回归：test/scenario/scenario_7310_sleep_crit.cpp（号段 7310-7319）。

#include "common.h"

namespace {

constexpr int k95AuditBase = 990095000;  // +owner：本次申报加成（百分点×100）

// ── 95 —— 对手处于睡眠状态时，致命一击率提升 {0}/16 ──
// int_args = [owner, target, sixteenths]（args_num=1；选择期路由布线同 747）。
EffectResult effect_95_sleep_crit_gate(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 95, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 95, args,
                     "95 布局=[owner,target,sixteenths]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int sixteenths = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 95, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(sixteenths > 0, 95, args, "需要 sixteenths>0");

    const bool sleeping = g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, opp, static_cast<int>(AbnormalStatusId::Sleep));

    // 审计槽：申报加成百分点×100（4/16=25% → 2500；不成立 → 0）
    ctx->plugin_storage[k95AuditBase + owner] = sleeping ? (625 * sixteenths) : 0;

    if (sleeping) {
        // +{0}/16 百分点（加算通道；每回合 reset，本回合被选中即重写）
        ctx->ws.crit_rate_add[owner] += 6.25f * static_cast<float>(sixteenths);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(95, effect_95_sleep_crit_gate)
