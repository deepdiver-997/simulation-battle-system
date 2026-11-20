// G1 · 消回合成功族三批 —— 990×28 / 1179×27 / 1083×28（2026-09-29，worktree ../agents/g1）
//
// E2（e_break_round.cpp）/ F1（f_break_round2.cpp）的直系后续批：同一家族形状——
// break_round_effects 返回 SUCCESS（真的清掉了东西）才执行后续子句；
// NO_EFFECTS（对方本来就没有）/ IMMUNE（免断）都不算成功。
//
// 990（args_num=1）"消除对手回合类效果，消除成功则附加{0}点固定伤害"：
//   - F1 1036 计次附伤的**即发版**（无"下N次"窗口）：SUCCESS → deal_pink_damage
//    （FIXED 档，吃护罩/固定抗性——粉伤通道是插件造固定伤害的唯一正规入口）。
//     固定伤害落在本次红伤之后（SKILL_EFFECT 桶晚于 ATTACK_DAMAGE 管线，1036 时点
//     注释同源）——"附加"语义一致。
//   - 载体（28 个，切片自查全为 1 参）：鸣世斩天诀 19756 [300]、绝代倾城 19871 [300]、
//     终极锁定 30050 [200]……4399 交叉验证：19756"必中；消除对手回合类效果，消除成功
//     则附加300点固定伤害"（=切片[300]）；19871"先制+3；……消除成功则附加300点固定伤害"。
// 1179（args_num=2）"消除对手回合类效果，消除成功则自身下{0}次攻击技能造成的伤害额外提升{1}%"：
//   - 直系先例 = **1850**（batch_c_skills.cpp"消除双方回合类效果，消除任意一方成功则
//     自身下{0}次攻击技能造成的伤害提升{1}%"，措辞仅差"双方/额外"）：SUCCESS →
//     AFTER_ACTION 一次性节点（FIRST/SECOND 双桶 + acting side 过滤 + 当回合守卫）
//     → 内核计数通道 grant_attack_boost（**授予 {0}+1 次**，补齐内核"消费先于应用"
//     的时点差，见 effect_1179_grant_flush 内注）。
//   - 为什么走内核通道而不是自挂管线回调（通道查准，2026-09-29 定稿）：
//     ① FSM 实测时点链是 ON_SKILL_HIT → **SKILL_EFFECT → ATTACK_DAMAGE**——SKILL_EFFECT
//       里直接授予/自挂 AMP_EXTRA 条目都会落进**自己这次攻击**的结算并被当次消费，
//       官方"**下**{0}次攻击"语义作废（1850 头注同款结论）→ 必须 AFTER_ACTION 延后；
//     ② grant_attack_boost 是"下N次攻击伤害提升X%"的**内核正道**：计伤时按
//       `base*(100+Σpct)/100` 累加（battleFsm.cpp 计伤段）、"成功使用攻击技能后"
//       逐槽消费（consume_attack_boost_grants_after_attack，正好咬合模板的"攻击技能"
//       限定——属性技/miss/被封属不消费）、换宠即清（attack_boost_grants 非继承）；
//     ③ "额外提升"的乘算差只在**同槽叠加**时可见（内核通道 Σ 后一次乘算，1850 同
//       行为）——家族一致性优先，偏差记档。
//   - 载体（27 个）：王·擎天之光 30809 [2,100]、蛮荒天威斩 31025 [2,100]……
//     4399 交叉验证：30809"消除对手回合类效果，消除成功则自身下2次攻击造成的伤害
//     提高100%"（4399 转述掉"额外"二字，effect_info 模板为权威）。
// 1083（args_num=0）"若后出手则消除对手回合类效果"：
//   - **纯消除**、无后续子句：acting_side_is_first 取反门槛（986"先出手时"同判据、
//     方向相反）→ break_round_effects；无需 SUCCESS 门槛（消除动作本身即全部行为）。
//   - SKILL_EFFECT 桶默认时点镜像 FIRST/SECOND（skills.cpp state_for_owner），
//     后手方的桶跑在 BATTLE_SECOND_SKILL_EFFECT → acting_side_is_first()==false。
//   - 载体（28 个）：痴愚 13739、谄诳 13740（无为觉者 4661 双五技；4399 无该二技条目
//     ——第五技常缺，effect_info 模板即权威）、恐惧黑洞 32193 等。⚠️ 13739/13740
//     moves.max_pp=0（pp=0 不可选，skills.cpp query_selectable）——场景验证改用同族
//     单效果载体 恐惧黑洞 32193 [1083]（分类切片全 28 载体无参一致，见
//     test/scenario/scenario_710_break_round3.cpp 头注）。
//
// 前置三查（2026-09-29）：①内核无 990/1179/1083 硬编码（grep src/、include/ 无命中）
// ②effect_meta 无该三 id 声明 ③全库无既有实现（无 SKILL_EFFECT(990/1179/1083)）。
//
// 场景回归：test/scenario/scenario_710_break_round3.cpp（场景段 710-719）。

#include "common.h"

namespace {

// ── 990 —— 消除对手回合类效果，消除成功则附加{0}点固定伤害 ──
EffectResult effect_990_break_then_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 990, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 990, args,
                     "需要 [owner,target,fixed]");
    EFFECT_ARG_GUARD(g_core && g_core->break_round_effects && g_core->deal_pink_damage,
                     990, args, "需要内核原语 break_round_effects/deal_pink_damage");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && fixed > 0,
                     990, args, "需要双方侧位合法且 fixed>0");
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不附加（家族门槛）
    }
    // FIXED 档粉伤：吃护罩/固定抗性（粉伤通道；伤害类型口径见常识速查 §三）。
    (void)g_core->deal_pink_damage(ctx, opp, fixed, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 1179 共用：AFTER_ACTION 一次性节点 → 内核计数通道授予（1850 grant_flush 同款）──
// bucket_acting_side：batch_c 匿名空间辅助不可跨 TU → 本地复刻（preemptive_right 判先手方）。
int g1179_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

EffectResult effect_1179_grant_flush(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int arm_round = args.int_args[3];
    if (owner < 0 || owner > 1 || g1179_bucket_acting_side(ctx) != owner
        || arm_round != ctx->roundCount) {
        return EffectResult::kOk;   // 只在"自己出手流程结束"且当回合授予
    }
    // ⚠️ 授予 {0}+1 次（场景实测口径）：内核**消费点在 query_usage（攻击流程早段）、
    // 应用点在 ATTACK_DAMAGE**（skills.cpp:1147 → battleFsm.cpp:619，消费先于应用）——
    // AFTER_ACTION 授予的条目会被"下一次攻击"的消费先扣 1 次而不应用 → 授予
    // {0}+1 次才精确命中"下{0}次攻击"（miss/被封不消费、属性技不消费，次数不空耗）。
    // ⚠️ 顺带记档（非本批范围）：1850 同通道在 AFTER_ACTION 授 count=1（batch_c
    // skills.cpp:1226），按同一时点序实际命中 0 次攻击——T7 只断言了授予结构未验证
    // 应用，属潜在 off-by-one，待其本人复核。
    ctx->grant_attack_boost(owner, 1179, /*count=*/args.int_args[1] + 1,
                            /*pct=*/args.int_args[2]);
    return EffectResult::kOk;
}

// ── 1179 —— 消除对手回合类效果，消除成功则自身下{0}次攻击技能造成的伤害额外提升{1}% ──
EffectResult effect_1179_break_then_counted_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1179, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1179, args,
                     "需要 [owner,target,times,pct]");
    EFFECT_ARG_GUARD(g_core && g_core->break_round_effects, 1179, args,
                     "需要内核原语 break_round_effects");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int times = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1
                         && times > 0 && pct > 0,
                     1179, args, "需要双方侧位合法且 times>0 && pct>0");
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;   // 消除失败 → 不武装计数
    }
    // AFTER_ACTION 延后授予（1850 同款）：FIRST/SECOND 双桶各挂一条、duration=1（只在
    // 用技当回合存活）、acting side 过滤保证只跑自己那条——落点晚于本次攻击的计伤与
    // 消费，"下{0}次"不含当次。
    const std::vector<int> payload{owner, times, pct, ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1179, 0, owner, 1, EffectArgs(payload), &effect_1179_grant_flush);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1179;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1083 —— 若后出手则消除对手回合类效果 ──
EffectResult effect_1083_later_break_round(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1083, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1083, args,
                     "需要 [owner,target]");
    EFFECT_ARG_GUARD(g_core && g_core->break_round_effects, 1083, args,
                     "需要内核原语 break_round_effects");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1,
                     1083, args, "需要双方侧位合法");
    if (acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // "若后出手"：先手方不触发（986 同判据反向）
    }
    // 纯消除、无后续子句 → 不需要 SUCCESS 门槛（NO_EFFECTS/IMMUNE 时消除本身无行为）。
    (void)g_core->break_round_effects(ctx, opp);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(990, effect_990_break_then_fixed)
SKILL_EFFECT(1179, effect_1179_break_then_counted_amp)
SKILL_EFFECT(1083, effect_1083_later_break_round)
