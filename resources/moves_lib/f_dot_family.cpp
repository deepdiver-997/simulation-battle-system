// F 组批产 · F7 概率持续固伤族（2026-09-29；工单：docs_local/docs/05-任务清单/F组批量效果工单.md）
//
//   3 效果一个家族（"概率 + 持续性伤害"的三种形态）：
//     603 {0}%概率使对手陷入{1}状态{2}回合              （切片 [chance,anomaly,rounds]）
//     76  {0}%概率令对手在{1}回合内每回合受到{2}点固定伤害（切片 [chance,rounds,per_round]）
//     20  {0}%令本方疲惫，{1}回合无法攻击                （切片 [chance,rounds]；**自伤技**）
//
//   载体（moves 表切片，2026-09-29 查库；side_effect/arg 均为 JSON 数组、按 effect_info.args_num
//   + side_effect.arg_count 切分，[0]=owner、[1]=target 由引擎绑定）：
//     603×34：异元幻空斩 16349 [195,603]|[100,16,2]（100% 流血2回合）、飞沙诀 16385 [603]|[25,16,1]
//             （25% 流血1回合）、恐惧冥光 23764、星碎 26085（瘫痪）、缚魂之念 26604（失神）…
//     76×38：痛苦撕裂 11143 [76]|[100,5,100]（100% 5回合×100）、沙暴铁碎阵 11146 [76]|[10,3,25]
//             （10% 3回合×25）、伤口撕裂 15618、至尊锋刃 13979…
//     20×34：火箭头槌 10072 [20]|[100,1]、光栅炮 10073 [20]|[100,1]（100% 疲惫1回合）、
//             百万吨冲顶 10693 [5,1]、巨大阴影 37711 [100,2]…（另有一批 [43,20] 属性技：
//             前缀 43 吸血 slice [1]，20 slice [100,1]，本文件不涉 43）
//   4399 原文（reference/scrape_4399/out/pets_full.jsonl，2026-09-29 grep）：
//     异元幻空斩"无视对手防御提升；100%使对手流血2回合"/飞沙诀"先制+2；25%概率使对手陷入
//     流血状态"/痛苦撕裂"100%概率令对手在5回合内每回合受到100点固定伤害"/沙暴铁碎阵
//     "10%对方三回合内受到25点伤害"/火箭头槌"100%令本方疲惫，1回合无法攻击"
//     ——76 族官方语义 = 独立"流血/撕裂"型持续固伤（**非异常状态**），与 603 的异常通道分开建模。
//
//   前置三查（2026-09-29）：①内核 grep 无 603/76/20 硬编码（防 697 式假阳性——skills.cpp /
//   battle_primitives.cpp / effect.cpp 无命中）②effect_meta.cpp 无分类命中 ③全库 grep
//   SKILL_EFFECT((603|76|20)) 无既有注册——真缺口。effect 22（令对手疲惫，灵魂干涉）在
//   damage_modifier.cpp 已实现，是**对手向**的姊妹模板，与本文件 20（本方向）方向相反。
//
//   实现口径：
//   · 603：SKILL_EFFECT 时点（miss/打盔不进本桶——731 先例，无需命中门）→ apply_anomaly_checked
//     **申报概率**（亮节闸门管辖，ChanceSource::Skill；掷骰在原语内、闸门改写之后）+
//     **指定时长 {2}**（本模板 {2} 是显式回合数，非 random_anomaly_duration——工单明示；
//     603 全部 34 载体均带 {2}）。异常码 {1} 全域 0..43，is_valid_abnormal_status_id 校验。
//   · 76：概率门 roll_percent（本效果无异常可申报、无 checked 通道——工单口径
//     "apply_anomaly_checked/roll_percent"二选一，非异常概率走本地掷）→ register_round_effect
//     挂 **BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS（回合扣减点 = 官方"每回合结束后"**，与
//     tick_abnormal_statuses 的异常 dot 结算同点、且在 cleanup 之前跑）每回合
//     deal_pink_damage(FIXED, {2})。窗口 = InRounds（"N回合内"家族口径：先手当回合即结算
//     第 1 次、后手由 round_effect_start_round 顺延一回合——663/615/1676 同款，两向都恰 N 次）。
//     门槛：目标存活（837/1148 同款 hp>0 早退）。
//   · 20：**自伤技**——SKILL_EFFECT 时点对自己 roll → apply_anomaly_checked(self, 疲惫7)。
//     "疲惫=无法攻击"的引擎语义：Fatigue(7) 是控制类异常（is_control_abnormal_status），
//     has_blocking_control_status 在每次 ACTION_START 整段禁主流程（battleFsm.cpp
//     should_skip_action_flow）——**施加即完整表达**，无需借 721/沉默的 skip 通道。
//     ⚠️ 时长 +1（记档）：roundCount **回合进行中 0 基**（advanceRound 在 Round Completion
//     才走——battleFsm.cpp handle_BattleRoundCompletion），自伤施加发生在日历回合 R 的
//     **自己行动内**（roundCount=R-1，本回合 ACTION_START 已过）→ duration=d 的
//     end=(R-1)+d 只覆盖到日历 R+d-1，而 R 的行动已花掉 → 未来被封的 ACTION_START 只有
//     R+1..R+d-1 共 d-1 个。"{1}回合无法攻击"（火箭头槌 100 威力自伤技的代价 = 下回合
//     跳过）须封 {1} 个**未来**行动 → 传 duration = {1} + 1。同构先例：derive_anomaly 对
//     "回合扣减点施加"的 +1 修正（battle_primitives.cpp："转出异常的第一个生效回合是下一回合"）。
//     对照：**对手向**的 effect 22 先手施加时对手当回合尚未行动、as-is 语义成立
//     （damage_modifier.cpp 直传 rounds）——方向不同，不可混用。
//     （603 无需 +1：异常 {2} 回合按 apply_anomaly 既有口径 = 施加回合算第 1 回合，
//     与官方"陷入流血2回合"= 当回合起算的语感一致，场景 670 T1 按 2 次 ActionStart 结算钉住。）
//
//   记档（口径待实测翻案只改一处）：
//   · 76 的 dot 挂**目标侧**（side 域，随目标侧换宠转移）——与异常 per-side 口径一致；
//   · 76 窗口走家族默认 ON_STAGE scope（可被断回合清除、施放方下场窗口失效）——与
//     moves_lib 全部回合窗（915/979/1219…）同口径；
//   · 76 的 10% 载体（11146）与 20 的低概率载体（10693 5% 等）不另设统计场景：
//     与 100% 载体同一 handler、同一掷点通道，概率门行为由场景 670 T2（25% 飞沙诀循环）
//     侧证。
//
//   场景回归：test/scenario/scenario_670_dot_family.cpp（603 用 100% 载体 16349 + 25% 载体
//   16385 循环统计；76 用 100% 载体 11143 精确 5×100；20 用 100% 载体 10072 验证
//   R2 封行动 / R3 解封）。

#include "common.h"

namespace {

// ── 603 —— {0}%概率使对手陷入{1}状态{2}回合。Args: [owner, target, chance, anomaly, rounds]。──
EffectResult effect_603_chance_anomaly_fixed_rounds(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 603, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 603, args, "需要 [owner,target,chance,anomaly,rounds]");
    EFFECT_ARG_GUARD(g_core && g_core->apply_anomaly_checked != nullptr, 603, args,
                     "需要 apply_anomaly_checked 原语");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 603, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 603, args, "异常码 {1} 越界");
    EFFECT_ARG_GUARD(rounds > 0, 603, args, "时长 {2} 非正");
    // 概率申报（亮节闸门管辖）+ 指定时长（非 random_anomaly_duration）；免疫/抗性/转化/
    // 弹控全在原语内按序处理，ROLL_FAILED 即闸门改写后仍未过。
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, rounds, owner, chance,
                                        ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 76 tick —— 回合扣减点结算：每回合 {2} 点固定伤害（粉伤·固定档）。──
// tick args = [owner, target, per_round]。
EffectResult effect_76_dot_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 76, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 76, args, "需要 [owner,target,per_round]");
    EFFECT_ARG_GUARD(g_core && g_core->deal_pink_damage != nullptr, 76, args,
                     "需要 deal_pink_damage 原语");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int per_round = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 76, args,
                     "owner/target 越界");
    ElfPet& foe = ctx->getPet(target);
    if (foe.hp <= 0) {
        return EffectResult::kOk;   // 目标已阵亡：dot 停（837/1148 同款）
    }
    (void)g_core->deal_pink_damage(ctx, target, per_round, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── 76 —— {0}%概率令对手在{1}回合内每回合受到{2}点固定伤害。──
// Args: [owner, target, chance, rounds, per_round]。
EffectResult effect_76_chance_rounds_dot(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 76, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 76, args, "需要 [owner,target,chance,rounds,per_round]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int rounds = args.int_args[3];
    const int per_round = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 76, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(rounds > 0 && per_round > 0, 76, args, "rounds/per_round 非正");
    if (!roll_percent(ctx->rng(), chance)) {
        return EffectResult::kOk;   // 概率门未过：不挂窗（76 无异常可申报 → 本地掷骰）
    }
    // "N回合内"窗口（InRounds 家族口径，见文件头）：回合扣减点每回合结算一次。
    const std::vector<int> tick_args{owner, target, per_round};
    register_round_effect(ctx, State::BATTLE_ROUND_REDUCTION_ALL_ROUND_MINUS, owner, 76, rounds,
                          tick_args, &effect_76_dot_tick);
    return EffectResult::kOk;
}

// ── 20 —— {0}%令本方疲惫，{1}回合无法攻击（自伤技）。Args: [owner, target, chance, rounds]。──
EffectResult effect_20_self_fatigue(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 20, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 20, args, "需要 [owner,target,chance,rounds]");
    EFFECT_ARG_GUARD(g_core && g_core->apply_anomaly_checked != nullptr, 20, args,
                     "需要 apply_anomaly_checked 原语");
    const int owner = args.int_args[0];
    const int chance = args.int_args[2];
    const int rounds = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1, 20, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0, 20, args, "时长 {1} 非正");
    // 自伤：目标=自己、actor=自己；概率申报走 checked 槽（亮节闸门管辖）。
    // 时长 +1 修正（自伤施加在自己行动内，当回合 ACTION_START 已过），见文件头记档。
    (void)g_core->apply_anomaly_checked(ctx, owner, static_cast<int>(AbnormalStatusId::Fatigue),
                                        rounds + 1, owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(603, effect_603_chance_anomaly_fixed_rounds)
SKILL_EFFECT(76, effect_76_chance_rounds_dot)
SKILL_EFFECT(20, effect_20_self_fatigue)
