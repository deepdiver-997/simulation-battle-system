// J 组 J314 · 连击无参常量族 ride —— 1608 / 1609 / 1610（2026-10-29，worktree ../agents/j314）
// J326 头注校正（2026-11-05）：主干来源由「内核常量旁路」改为「DB 行参+规则表」，
//   rider 算法一字不动（J326 仅同步注释；本文件三个函数体 J314 原样）。
//
// 家族语义（effect_info 模板即权威；三兄弟同文）：
//   1608/1609/1610（effect_info args_num=0——原文保留；side_effect.arg_count 已由
//     J326 db_patch j4 规范化 0→2）
//     「召唤自己的伙伴进行5-10次攻击，布布X发起时可额外令…」
//   主干（连击 5~10 次）= DB 数据（J326 数据刀清缴线二批，内核+DB 授权单）：
//     载体平铺参数 [100,1] → [100,1,5,10]（尾部两参=连击区间行参，本地规范化——
//     不声称官方曾提供）+ custom_skill_combo_rules 规则行 (0,1)
//     （tools/db_patches/j4/1608_combo_rules.sql）。原 skills.cpp combo_fixed_range
//     常量旁路 {5,10}（J314）与 combo_arg_indices 13 case 已随 J326 撤除，
//     内核只留一条通用取参规则（查行→校验下标→max(1,参数)→排序写 combo_min/max）。
//   本文件只承接 **ride 子句**（三个都是「下回合」窗，不涉 loadSkills selection_effects_
//   首回合表——速查 §四点五：这是用技期注册的 NextRounds 窗，非条件先制族）：
//     1608 布布鸟：下回合自身**所有技能**先制+3   （970 直系：NextRounds(1) FIRST_MOVE_RIGHT
//       tick → effect_move_right_priority_bonus（843/499 先制通道，common.h 共享体）；
//       preemptive_level 在 MOVE_RIGHT memset 后写，下一回合首手判定即读它）
//     1609 布布犬：下回合自身**攻击**造成的伤害翻倍（631 直系：NextRounds(1) ROUND_START
//       tick → ws.damage_add_pct[owner] += 100——「双倍/翻倍」= 通用增伤 +100%，
//       193/890 同通道；damage_add_pct 是 **owner 造成伤害** 的 AMP 通道
//       （bucket_owner==attackerId 才生效），ws 每回合 reset 天然单回合有效）
//     1610 布布熊：下回合**对手**属性技能失效     （1438/K311 直系：seal_skill(
//       source=owner, target=对手, attribute=true, attack=false, count=0（不限次数）,
//       duration_rounds=1（单回合窗）, penetrable=true, ON_STAGE, hit_invalid=false,
//       chance=100)——SEAL_ATTRIBUTE 非 hit_invalid，「失效」走技能无效补偿口径）
//
// 触发时点（majority reading 记档，J314 看板行「发起时」）：
//   rider 在**使用该技能当回合**的效果结算期武装（SKILL_EFFECT 桶、HIT 分支——
//   与 970/631/1438 全部先例同位：效果族默认命中才结算；miss/被无效当回合不武装，
//   载体 34454/34457/34460 must_hit=1 实战不构成偏差点）。窗口一律 NextRounds(1)
//   （本回合不算，下回合生效、次回合消失——register_next_rounds_effect 窗骨，
//   J55/J141 同款）。
//
// 载体（全库唯一引用面，moves.side_effect=[451,160x]|[100,1,5,10]）：
//   34454 玩偶合力击 [451,1608] / 34457 [451,1609] / 34460 [451,1610]——物30 must_hit。
//   链兄弟 451（100% 随机三异常，h_chance_anomaly.cpp 已实现）与 ride 无 rand 交集
//   （451 的种类 roll 在自身 handler 内、ride 不掷点；451 切片 [100,1] 前序不受
//   160x 尾部行参影响——build_skill_effects 按链序 arg_count 切片）。
//
// int_args 布局（J326 起）：[owner, target, 5, 10]——装载器注入 owner/target 占位后
//   追加本效果局部切片 [5,10]（build_effect_args_for_skill）。三个 rider 只读
//   int_args[0]/[1]，尾部行参不改变读取（EFFECT_ARG_GUARD 仍验 >=2）。
//
// 动工前置三查（2026-10-29 J314 原档）：①内核 grep 1608/1609/1610 仅 effect_meta Combo
//   标注行 + combo_arg_indices 头注释「未覆盖」行，无行为硬编码（J326 起连这两处开关
//   也已撤）；②effect_meta 三 id 已标 EffectCategory::Combo；③moves_lib 并集无
//   SKILL_EFFECT(160x) 注册——J314 补上，本文件即其注册点。
// 先例：970（NextRounds MOVE_RIGHT 先制 tick，j_reversal_misc_gates）、631（NextRounds
//   ROUND_START 增伤 tick，j_cure_next_amp）、1438（seal_skill 属性封窗，
//   j_negative_seal_attribute）、K311 323（封属=SEAL_ATTRIBUTE+补偿口径，soul_lib）、
//   193（「双倍」=damage_add_pct+100，j_status_double_damage）。
// 场景回归：test/scenario/scenario_7480_combo_constant.cpp（号段 7480-7489）+
//   test/scenario/scenario_7910_combo_rules_data.cpp（J326 行参/规则/覆盖/禁用/负测，7910-7919）。

#include "common.h"

namespace {

// ── 1609 tick：下回合自身攻击伤害翻倍（ROUND_START 桶写 AMP，ws 每回合 reset）──
EffectResult j314_1609_amp_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[self] += 100.0f;   // 翻倍 = 通用增伤 +100%（193/890 通道）
    return EffectResult::kOk;
}

// ── 1610 tick：下回合对手属性技能失效（ROUND_START 挂单回合封属窗）──
EffectResult j314_1610_seal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    (void)g_core->seal_skill(ctx, /*source=*/owner, /*target=*/opp, /*effect_id=*/1610,
                             /*attribute=*/true, /*attack=*/false, /*count=*/0,
                             /*duration_rounds=*/1, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE,
                             /*hit_invalid=*/false, /*chance_pct=*/100,
                             /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

}  // namespace

// ── 1608 —— 连击5-10次（DB 行参+规则表，J326）；布布鸟发起 → 下回合自身所有技能先制+3 ──
// int_args = [owner, target, 5, 10]（尾部两参=连击区间行参；本函数只读 [0]/[1]）。
EffectResult effect_1608_combo_bird_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1608, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1608, args,
                     "1608 布局=[owner,target,5,10]（J326 起尾部=连击区间行参）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1608,
                     args, "需要 owner/target ∈ {0,1}");
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, owner, 1608,
                                /*rounds=*/1, {owner, opp, /*prio=*/3},
                                &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── 1609 —— 连击5-10次（DB 行参+规则表，J326）；布布犬发起 → 下回合自身攻击伤害翻倍 ──
EffectResult effect_1609_combo_dog_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1609, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1609, args,
                     "1609 布局=[owner,target,5,10]（J326 起尾部=连击区间行参）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1609,
                     args, "需要 owner/target ∈ {0,1}");
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1609,
                                /*rounds=*/1, {owner, opp}, &j314_1609_amp_tick);
    return EffectResult::kOk;
}

// ── 1610 —— 连击5-10次（DB 行参+规则表，J326）；布布熊发起 → 下回合对手属性技能失效 ──
EffectResult effect_1610_combo_bear_seal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1610, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1610, args,
                     "1610 布局=[owner,target,5,10]（J326 起尾部=连击区间行参）");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp, 1610,
                     args, "需要 owner/target ∈ {0,1}");
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1610,
                                /*rounds=*/1, {owner, opp}, &j314_1610_seal_tick);
    return EffectResult::kOk;
}

SKILL_EFFECT(1608, effect_1608_combo_bird_priority)
SKILL_EFFECT(1609, effect_1609_combo_dog_amp)
SKILL_EFFECT(1610, effect_1610_combo_bear_seal)
