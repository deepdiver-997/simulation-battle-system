// J 组 J52 · 计数输出族 —— 3 宏/54 载体（2026-10-03，worktree ../agents/j52）
//
// 家族语义（effect_info 模板即权威）：
//   1328 [4] {0}%的概率造成{1}倍伤害，每次使用概率增加{2}%，最高概率{3}%
//   768  [1] 对手每处于一种异常状态则附加{0}点固定伤害
//   848  [1] 对手每处于一种能力下降状态附加{0}点固定伤害
//
// 实现口径：
//   - 1328 = 782 递增范式直系（f_ramp_family.cpp）：uses 计数存 plugin_storage
//     （owner×技能 分账，SKILL_EFFECT 每用 +1、跨回合不重置、换技不断账——782 已
//     定口径），第 n 用申报概率 = min(base+(n-1)*step, cap)（首用=base）；命中申报
//     概率 → ws.damage_add_pct[owner] += (mult-1)*100（{1}倍 = +({1}-1)×100 加算，
//     SKILL_EFFECT 时点在伤害管线 AMP 之前，batch_b/d5_D10 同路）。1328 是伤害类
//     概率、无"落地"可闸，掷骰走插件本地 roll_percent（482 同层，工单口径）。
//     审计槽双写：申报概率 + 本用是否命中（F9 782 首创，场景无随机断言用）。
//   - 768 = 逐异常计数 ×{0} 固伤（2145 遍历口径 0..kOfficialAbnormalStatusMaxId
//     逐 has_active_anomaly，"种"= 活跃异常 id 数；413 同款 g_core->fixed_damage
//     直打）。载体切片 [1]。
//   - 848 = 768 同构：六槽负槽计数（j51_count_boost_kinds 取反判 <0，"种"= 负槽
//     槽数、与降幅无关）×{0} fixed_damage。
//
// 载体切片（unity 库 json_each 精确匹配 18+27-9(1768 误配)+19=54 计账口径一致；
// 1328 纯链载体：32909 星辰腐朽 [10,3,10,30]/33908 亡灵泯灭杀 [5,2,5,15]，其余
// 16 条全链兄弟效果已实现（1180/636/572/827 等在库）——场景只打纯链，防"链激活"
// 翻车（速查 §五）。768 纯链：37111 破壁蚀崖 [200]；848 纯链：35097 理性追逐 [50]。
//
// 动工前置三查（2026-10-02/03）：①内核 grep 1328/768/848 无硬编码 ②
// custom_effect_overrides 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1380_count_output.cpp（号段 1380-1389）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

// ── plugin_storage 键段（990152x，grep 全库无撞）──
constexpr int k1328UsesKeyBase = 990152100;    // +owner*40000 + skill_id%40000：已用次数
constexpr int k1328ChanceAuditBase = 990152200;   // +owner：本次申报概率（审计槽）
constexpr int k1328ProcAuditBase = 990152300;     // +owner：本用是否命中（审计槽，1/0）

// ── 1328 —— {0}%概率造成{1}倍伤害，每次使用概率+{2}%，最高{3}% ──
// int_args = [owner, target, base, mult, step, cap]（args_num=4）。
EffectResult effect_1328_ramp_multiplier(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1328, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1328, args,
                     "1328 布局=[owner,target,base,mult,step,cap]（int_count>=6）");
    const int owner = args.int_args[0];
    const int base = args.int_args[2];
    const int mult = args.int_args[3];
    const int step = args.int_args[4];
    const int cap = args.int_args[5];
    if (owner < 0 || owner > 1 || base < 0 || mult <= 1 || step <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    // uses 计数（owner×技能 分账；取不到技能槽退化为 skill_id=0 独立账本，782 同款）
    const int slot = ctx->executing_skill_slot(owner);
    const int skill_id = (slot >= 0 && slot < 5)
                             ? ctx->getPet(owner).skills[static_cast<std::size_t>(slot)].id
                             : 0;
    const int key = k1328UsesKeyBase + owner * 40000 + (skill_id % 40000);
    const auto it = ctx->plugin_storage.find(key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[key] = uses;
    const int chance = std::min(base + (uses - 1) * step, cap);   // 首用 = base
    ctx->plugin_storage[k1328ChanceAuditBase + owner] = chance;
    const bool proc = roll_percent(chance);
    ctx->plugin_storage[k1328ProcAuditBase + owner] = proc ? 1 : 0;
    if (proc) {
        // {1}倍 = +({1}-1)×100 加算百分点（damage_add_pct 是加算通道，×3 → +200）
        ctx->ws.damage_add_pct[owner] += static_cast<float>((mult - 1) * 100);
    }
    return EffectResult::kOk;
}

// ── 768 —— 对手每处于一种异常状态则附加{0}点固定伤害 ──
// int_args = [owner, target, fixed]（args_num=1）。
EffectResult effect_768_per_anomaly_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 768, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 768, args,
                     "768 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     768, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 768, args, "需要 固伤 > 0");
    if (!g_core || !g_core->has_active_anomaly || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    int kinds = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, target, id)) {
            ++kinds;   // "一种"= 活跃异常 id 数（2145 遍历口径）
        }
    }
    if (kinds > 0) {
        (void)g_core->fixed_damage(ctx, target, kinds * fixed);
    }
    return EffectResult::kOk;
}

// ── 848 —— 对手每处于一种能力下降状态附加{0}点固定伤害 ──
// int_args = [owner, target, fixed]（args_num=1）。
EffectResult effect_848_per_drop_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 848, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 848, args,
                     "848 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     848, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 848, args, "需要 固伤 > 0");
    int kinds = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[target][i] < 0) {
            ++kinds;   // "一种"= 负槽槽数（与降幅无关）
        }
    }
    if (kinds > 0 && g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, target, kinds * fixed);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1328, effect_1328_ramp_multiplier)
SKILL_EFFECT(768, effect_768_per_anomaly_fixed)
SKILL_EFFECT(848, effect_848_per_drop_fixed)
