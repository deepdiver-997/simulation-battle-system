#include <utils/sbs_rand.h>
// J 组 J135 · 变威力族二 —— 61×11 / 70×13（+100×12 记档缓办）（2026-10-04，
// worktree ../agents/j135）
//
// 家族语义（effect_info 模板即权威；4399 载体逐字同文）：
//   61（0 参）"威力随机，随机范围50~150"   —— 峰回路转 10484、未来射线 10747…
//   70（0 参）"威力随机，随机范围150~220"  —— 远古审判 10847、压迫气息 11528…
//   100（0 参）"自身体力越少则威力越大" —— ⛔ 记档缓办：载体 base power=0、模板/图鉴/
//        DB moves.info 三源均无任何量级/阶梯数字（凶险幻象/搏杀意志/背水之击），实现
//        即纯造口径——挂账待语料补齐后再动（J1 记档先例）。
//
// 实现口径：
//   - 102 变威力通道直系（J102 1252 同文件系先例）：SKILL_EFFECT 时点
//     ws.skill_power_view 已被引擎物化为基线威力（ON_SKILL_HIT materialize）→
//     掷 band 整数直写 view（operator= 置 rewritten → 标准变威力重算管线）。
//   - ⚠️ 随机威力载体 base power 全为 0（带即威力，DB 实测 6/6 载体 power=0）——
//     物化的基线视图是 0，**不能**套 J102 的 view.value<=0 守卫（会恒短路）；
//     无条件直写（miss 时无伤害阶段，写视图无副作用）。
//   - band 闭区间：[50,150] = 50 + sbs_rand()%101；[150,220] = 150 + sbs_rand()%71。
//
// 载体切片（unity 库 json_each + side_effect.arg_count 脚本核算 2026-10-04）：
//   61  0 参：纯链×11（10484 峰回路转 物、10747 未来射线 特、11158 风暴召唤…）
//   70  0 参：纯链×8（10847 远古审判 特、11528 压迫气息 特、11893 浑天圆舞 物…）
//   100 0 参：纯链×10（11703/12101/12283…power=0，记档见上）
//
// 动工前置三查（2026-10-04）：①内核 grep 61/70/100 无硬编码 ②effect_meta 无分类牵连
// ③moves_lib 无既有注册——真缺口（skill_power_view 既有设施）。
// 场景回归：test/scenario/scenario_2300_random_power.cpp（号段 2300-2309）。

#include "common.h"

namespace {

// ── 61 —— 威力随机，随机范围50~150 ──
// int_args = [owner, target]（0 参效果，装载器注入）。
EffectResult effect_61_random_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 61, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 61, args,
                     "61 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 61, args, "需要 owner ∈ {0,1}");
    auto& view = ctx->ws.skill_power_view[owner];
    view = 50 + sbs_rand() % 101;   // [50,150] 闭区间（载体 power=0，带即威力）
    return EffectResult::kOk;
}

// ── 70 —— 威力随机，随机范围150~220 ──
EffectResult effect_70_random_power(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 70, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 70, args,
                     "70 布局=[owner,target]（0 参效果）");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 70, args, "需要 owner ∈ {0,1}");
    auto& view = ctx->ws.skill_power_view[owner];
    view = 150 + sbs_rand() % 71;   // [150,220] 闭区间（载体 power=0，带即威力）
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(61, effect_61_random_power)
SKILL_EFFECT(70, effect_70_random_power)
