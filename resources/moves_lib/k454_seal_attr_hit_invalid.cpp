// K454 · 属性技命中失效窗 —— 1838×6（2026-11-10，worktree ~/.agent-workspaces/workspaces/sbs-local/k454）
//
// 家族语义（effect_info 官方模板即权威）：
//   1838（args_num=1）"对手处于异常状态时{0}回合内对手属性技能命中效果失效"
//
// 载体切片（unity 库 json_each 精确匹配；全链 side_effect 表有 ac，顺序切分零剩余；
//   切片以加载器 build_effect_args_for_skill 为准，下行清单为核对记录）：
//   1838 [{0}=回合]：29335 斗争宣言[191(1),1838(1),2040(3)]=[4|2|2,100,2]、
//     36027 夜荧拂天歌[942(1),1838(1),977(2)]=[4|2|4,2]、36324 天兔澄灵阵[1838(1),
//     1709(1),1911(1)]=[2|3|250]、36953 逆鳞破斩[1878(1),1838(1),769(1)]=[3|2|50]、
//     38281 啸风穿云击[1034(1),553(2),1838(1),984(1)]=[1|100,7|1|1]（**N=1**）、
//     38324 生机衰弱波[1808(1),1838(1),1668(1),897(2)]=[50|2|15|1,50]。
//     ⚠️ 38281 为 N=1、其余 N=2——实现读行参，不硬编码 2（2026-11-10 复核纠错：
//     首报曾误写"六行全部=[2]"，38281 实为 [1]）。
//
// 实现（默认 SKILL_EFFECT 时点，无路由/无内核改动）：
//   1. 条件门：ctx->has_any_active_abnormal_status(target)（2009/1048 同判据，
//      batch_a_skills.cpp:748）——"对手处于异常状态时"在**生效时刻**求值；
//      未触发 = 不授予（1838 文本无回退子句）。
//   2. 授予 = seal_skill(attribute=true, attack=false, count=0, duration_rounds={0},
//      hit_invalid=true) → RuleCenter SEAL_ATTRIBUTE_HIT：**技能照常命中、效果失效、
//      不触发 SKILL_INVALID 补偿**（core_api.h:89-91 语义；≠ SEAL_ATTRIBUTE 技能无效，
//      2427/738 首用 shengguang_monika_skills.cpp:78-83 同参先例）。
//      ⚠️ 998（j_boost_lost_response.cpp）同措辞走 grant_skill_invalid SEAL_ATTRIBUTE
//      窗——按本单裁定视为待查存量缺陷，**不复制**；本单以 22410 灵魂之歌[136,15] 作
//      真实无效补偿对照（136 只在 miss/SKILL_INVALID 趟自愈 → SEAL_ATTRIBUTE_HIT 下
//      不自愈 = 非 SEAL_ATTRIBUTE 实证，场景 T8410）。
//
// 生命周期（seal_skill doc 定档）：
//   - 回合型每回合递减；"N回合内传 rounds 本身、不+1"（2009/2321/2097 定档）；
//   - 挂施放方 source（ON_STAGE）：施放方换宠清（离场清票、回场需重施放再武装）、
//     断施放方回合解封（rule_center_.invalidate_stale 随 invalidate_all_round_effects）；
//   - 条件只在施放时刻求值，窗开启后不随对手异常消失而撤。
//
// 动工前置三查（2026-11-10，k454 worktree + 主库现查）：
//   ①内核 grep 1838 无硬编码（src/ include/ 零命中）②side_effect 表 1838 ac=1 与
//   effect_info.args_num=1 一致、六载体零剩余 ③registry moves=1527 无 1838、
//   HUB+八 worktree grep SKILL_EFFECT(1838/SOUL_PROGRAM(1838/kMark1838 零命中——真缺口。
// 先例：2009（条件门+seal_skill 回合窗直系，差 hit_invalid 语义）、2427/738（hit_invalid
//   首用参形）、998（同措辞/异通道——存量待查，不复制）、2321/2097（InRounds 传法）。
// 场景回归：test/scenario/scenario_8410_k454.cpp（8410-8419 号段）。

#include "common.h"

namespace {

// ── 1838 —— 对手处于异常状态时{0}回合内对手属性技能命中效果失效 ──
// int_args = [owner, target, {0}=回合]（args_num=1）。
EffectResult effect_1838_cond_seal_attr_hit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1838, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1838, args,
                     "1838 布局=[owner,target,回合]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1838, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1838, args, "需要 回合 > 0");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    // 条件门："对手处于异常状态时"（生效时刻求值，2009/1048 同判据）
    if (!ctx->has_any_active_abnormal_status(target)) {
        return EffectResult::kOk;   // 未触发：不授予（本模板无回退子句）
    }
    (void)g_core->seal_skill(ctx, owner, target, /*effect_id=*/1838,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/rounds, /*penetrable=*/true,
                             /*source_slot=*/ctx->on_stage[owner], EffectScope::ON_STAGE,
                             /*hit_invalid=*/true, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1838, effect_1838_cond_seal_attr_hit)
