// J 组 J51 · 条件增伤/固伤族五 —— 3 宏/55 载体（2026-10-02，worktree ../agents/j51）
//
// 家族语义（effect_info 模板即权威）：
//   1103 [1] 若对手处于能力提升状态则造成的攻击伤害额外提升{0}%
//   179  [1] 若属性相同则技能威力提升{0}
//   413  [1] 若对手处于能力提升状态则附加{0}点固定伤害
//
// 实现口径：
//   - 1103 = 1198 镜像条件（J35）：对手**有正槽**（count_boost_kinds>0 判据）→
//     "额外提升"= **AMP_EXTRA 乘法通道**（693 判据字面：effect_set_damage_amp_extra
//     抢首空槽，J3 同款）——写入落本次攻击 snapshot。
//   ⛔ 179 记档跳过（2026-10-02 夜跑）：效果函数未被调用（纯载体 33567 [179]|[150]
//     亦然；与 J42 790 同因）——疑 parsed_units_ 对"若属性相同则"类条件单元的解析/
//     路由缺口（"若X则Y"被解析为条件单元后未见调用），R 单候选待白天排查。
//     （旁证：同系局红伤 1692 = 自然本系加成 ×1.5，与无 179 的预期一致。）
//   - 413 = 989 门镜像（J42）：对手有正槽 → fixed_damage({0})（DETECT 攻方桶直打）。
//   共用判据助手：count_boost_kinds（六槽正槽计数）>0 = "处于能力提升状态"。
//
// 载体切片（unity 库 json_each 精确匹配 18+18+18=54→计账 55；切片全载体自洽、零剩余，
// 脚本核算 2026-10-02）：
//   1103 [pct]
//   179  [提升点]：12981 机械能量炮 [20]、32220 仙气环绕 [30]、33567 萌喵电刃 [150]
//   413  [固伤]：13358 飞沙龙鸣击 [50]、13884 地影决灭震 [100]、15429 灵火破天击 [150]
//
// 动工前置三查（2026-10-02）：①内核 grep 3 id 无硬编码 ②custom_effect_overrides 无声明
// ③moves_lib 并集（全 worktree）无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1370_cond_amp5.cpp（号段 1370-1379）。

#include "common.h"

namespace {

// 自身正槽计数（batch_a/batch_c/J39 同判据：六槽含命中）。
inline int j51_count_boost_kinds(const BattleContext* ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 1103 —— 若对手处于能力提升状态则造成的攻击伤害额外提升{0}% ──
// int_args = [owner, target, pct]（args_num=1）。
EffectResult effect_1103_boost_gate_amp_extra(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1103, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1103, args,
                     "1103 布局=[owner,target,pct]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1103, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(pct > 0, 1103, args, "需要 pct > 0");
    if (j51_count_boost_kinds(ctx, target) <= 0) {
        return EffectResult::kOk;   // 门：对手处于能力提升状态
    }
    // "额外提升"= AMP_EXTRA 乘法通道（693 判据字面），落本次攻击 snapshot。
    EffectArgs tick_args(std::vector<int>{owner, target, pct});
    (void)effect_set_damage_amp_extra(ctx, tick_args);
    return EffectResult::kOk;
}

// ── 413 —— 若对手处于能力提升状态则附加{0}点固定伤害 ──
// int_args = [owner, target, fixed]（args_num=1）。
EffectResult effect_413_boost_gate_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 413, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 413, args,
                     "413 布局=[owner,target,固伤]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     413, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(fixed > 0, 413, args, "需要 固伤 > 0");
    if (j51_count_boost_kinds(ctx, target) <= 0) {
        return EffectResult::kOk;   // 门：对手处于能力提升状态
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, target, fixed);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1103, effect_1103_boost_gate_amp_extra)
SKILL_EFFECT(413, effect_413_boost_gate_fixed)
