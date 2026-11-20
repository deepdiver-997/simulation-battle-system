// J 组 J253 · 条件威力强化族 —— 2 宏/17 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   638（arg_count=2）"若对手{0}，技能威力提升{1}%"（{0}=官方异常码）
//   758（arg_count=1）"使用时若有一方处于{0}状态，则必定威力翻倍"（{0}=官方异常码）
//
// 实现口径（先例：j_boost_cond_amp.cpp 845 / tianqi_skills.cpp 97 通道）：
//   - 两条都在 SKILL_EFFECT 时点写 `ws.skill_power_view[owner]`（102 变威力通道；
//     物化在 ON_SKILL_HIT、效果在更晚的 SKILL_EFFECT 改，写=声明变威力 → ATTACK_DAMAGE
//     收尾重算）。view>0 才写（845 同守卫；强制执行打盔的 0 威力不复活）。
//   - 638 门 = 对手处于 {0} 异常（has_active_anomaly 原语查询）→
//     view += view × {1} / 100（{1} 在位即消费）。
//   - 758 门 = 自身或对手任一方处于 {0} 异常 → view ×2（97 通道"必定翻倍"不改写后清零，
//     与 845 的 += 区分；用绝对乘 2 而非 += view 避免重复实例叠加）。
//   - 审计槽（场景精确断门，9xxxx 私有段）：
//       996380100 + owner     = {1} 门过（已加成 pct）/ -1 门闭
//       997580100 + owner     = 触发侧位掩码（1=自身,2=对手,3=双方）/ 0 门闭
//
// 载体切片（side_effect 链按 effect_info.args_num 顺序切分，2026-10-22 脚本核算，17/17 自洽）：
//   638 [异常码,pct]：16735 迅影幻光爪（纯链）[1 中毒,50]、32950 掠影之角（纯链）[20 失明,50]、
//     37574 燃点爆发（纯链）[2 烧伤,200]、16340 妖瞳幻魔术 [9 石化,75]、
//     16388 无我逆沙舞 [16 流血,75]、16693 万古称帝斩 [1,50]、19389 万军惊魂斩 [2,100]、
//     33254 幻影毒尾刺 [1,100]、33642 枯枝叶伏击 [1,50]。
//   758 [异常码]：37844 漆黑扑击 [10 混乱]、33104 雪落无痕 [5 冻伤]、
//     17297/19002 焱皇烈焰 [2 烧伤]、32655 暗焰灼心击 [2]、36488 烬谷幽火咒 [2]、
//     37850 浴炎击 [2]、38425 褪喧哗的赤海 [2]。
//   ⚠️ 179（若属性相同则技能威力提升{0}）已由 effect_unit_parser.cpp SameElement+PowerBoost
//     内建（J88 记档），registry 不计 → 盘点假阳性，本单不重复立。
//
// 动工前置三查（2026-10-22）：①内核 grep 638/758 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 无行 ③全库 moves_lib/soul_lib 并集（含各 worktree）
//   无 SKILL_EFFECT(638/758) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_5840_cond_power_amp.cpp（号段 5840-5849）。

#include "common.h"

namespace {

constexpr int k638AuditBase = 996380100;   // +owner：门过写 pct / -1 门闭
constexpr int k758AuditBase = 997580100;   // +owner：侧位掩码 / 0 门闭

// 指定方是否处于官方异常 {0}（id 越界直接按门闭处理，防坏行参）。
bool j253_has_anomaly(BattleContext* ctx, int side, int anomaly_id) {
    if (!ctx || side < 0 || side > 1 || !g_core || !g_core->has_active_anomaly
        || !is_valid_abnormal_status_id(anomaly_id)) {
        return false;
    }
    return g_core->has_active_anomaly(ctx, side, anomaly_id);
}

// ── 638 —— 若对手处于{0}异常状态，技能威力提升{1}% ──
// int_args = [owner, target, 异常码, pct]（args_num=2）。
EffectResult effect_638_foe_status_power_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 638, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 638, args,
                     "638 布局=[owner,target,异常码,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     638, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 638, args, "需要合法官方异常 id");
    if (!j253_has_anomaly(ctx, target, anomaly)) {
        ctx->plugin_storage[k638AuditBase + owner] = -1;   // 门闭
        return EffectResult::kOk;
    }
    ctx->plugin_storage[k638AuditBase + owner] = pct;      // 在位即消费（显式 0 = 关闸档）
    const int view = ctx->ws.skill_power_view[owner];
    if (view > 0) {
        ctx->ws.skill_power_view[owner] = view + view * pct / 100;   // 变威力（102 通道）
    }
    return EffectResult::kOk;
}

// ── 758 —— 使用时若有一方处于{0}状态，则必定威力翻倍 ──
// int_args = [owner, target, 异常码]（args_num=1）。
EffectResult effect_758_either_status_power_double(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 758, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 758, args,
                     "758 布局=[owner,target,异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     758, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 758, args, "需要合法官方异常 id");

    int mask = 0;
    if (j253_has_anomaly(ctx, owner, anomaly)) {
        mask |= 1;
    }
    if (j253_has_anomaly(ctx, target, anomaly)) {
        mask |= 2;
    }
    ctx->plugin_storage[k758AuditBase + owner] = mask;
    if (mask == 0) {
        return EffectResult::kOk;   // 门闭
    }
    const int view = ctx->ws.skill_power_view[owner];
    if (view > 0) {
        ctx->ws.skill_power_view[owner] = view * 2;   // 必定翻倍（97 通道）
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(638, effect_638_foe_status_power_up)
SKILL_EFFECT(758, effect_758_either_status_power_double)
