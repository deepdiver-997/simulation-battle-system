// J 组 J271 · 每回合概率封属兜底族 —— 2 宏/12 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1458 [4] {0}回合内每回合{1}%对手属性技能无效，未触发则{2}%令对手{3}
//   1622 [4] {0}回合内每回合{1}%对手属性技能无效，未触发则下{2}次受到的攻击伤害减少{3}%
//
// 实现口径（先例：selection_seal.cpp 695 回合型封属；j_fallback_window2.cpp 1427
// 每回合 ROUND_START 掷点 + 首跳切分；heal_drain.cpp 1287 REDUCE_PCT 计次）：
//   - 每回合掷 {1}%：**施放当回合立即掷一次**（ROUND_START 已过，1427 首跳口径），
//     2..{0} 回合走 NextRounds 的 ROUND_START tick。
//   - 掷中 → seal_skill(封属性, count=0 窗口型, duration_rounds=1, chance_pct=100)
//     —— 当回合对手属性技能无效；掷歪 → 兜底：
//       1458 → apply_anomaly_checked(对手, {3}, chance={2})（申报制）；
//       1622 → REDUCE_PCT/MITIGATE 计次票（shared_ptr 倒计数 {2} 次，每次受击 ×(100-{3})%）。
//   - 审计槽：991458200+self = 本次封属武装(1/0)；991458300+self = 1458 异常落地(1/0)；
//     991622200+self = 1622 减伤票武装(1/0)。
//   - {1}/{2}/{3} 在位即消费：{1}=0/100 由 roll_percent 短路；{2}=0 时兜底不触发。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1458 [回合,p1,p2,异常]：36400 祖传秘方 [3,50,50,29]（纯链）、36516 迷离圣光
//     [3,50,100,8]（纯链）、26641 漫天华彩 [3,50,100,29]、27281 催眠粉尘 [4,50,100,28]、
//     28135 雷裁之心 [2,50,100,0]、28378 如喑墨穹 [3,66,100,30]、29205 上古岩碑
//     [3,50,100,29]、35363 万物终未济 [3,50,100,8]、37407 弥心之流 [2,50,100,15]。
//   1622 [回合,p1,次数,pct]：36677 杀意威慑 [2,50,1,50]（纯链）、27344 初始之源
//     [3,50,1,50]、27756 异世重构 [3,50,1,50]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1458/1622 无行为硬编码 ②effect_info 模板
//   唯一、custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集
//   （含 kbatch 与各 agents worktree）无 SKILL_EFFECT(1458/1622) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6020_chance_seal_fallback.cpp（号段 6020-6029）。

#include "common.h"

#include <memory>

namespace {

constexpr int kAudit1458Seal = 991458200;   // +self
constexpr int kAudit1458Anom = 991458300;   // +self
constexpr int kAudit1622Arm = 991622200;    // +self

bool anomaly_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
        case ApplyAnomalyResult::CONVERTED:
            return true;
        default:
            return false;
    }
}

// 1458/1622 共用前段：掷点按来源宏分账；封属票保留原有 1458 句柄。
bool j271_try_seal(BattleContext* ctx, int rule_id, int self, int opp, int chance) {
    if (!roll_skill_percent(*ctx, rule_id, 0, self, opp, chance)) {
        return false;
    }
    if (g_core && g_core->seal_skill) {
        (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/1458,
                                 /*attribute=*/true, /*attack=*/false, /*count=*/0,
                                 /*duration_rounds=*/1, /*penetrable=*/true,
                                 /*source_slot=*/-1, EffectScope::ON_STAGE,
                                 /*hit_invalid=*/false, /*chance_pct=*/100,
                                 /*consumed_when_pierced=*/false);
    }
    return true;
}

// ── 1458 单回合掷点 ──
void j271_1458_round(BattleContext* ctx, int self, int opp, int p1, int p2, int anomaly) {
    const bool sealed = j271_try_seal(ctx, 1458, self, opp, p1);
    ctx->plugin_storage[kAudit1458Seal + self] = sealed ? 1 : 0;
    if (sealed) {
        ctx->plugin_storage[kAudit1458Anom + self] = 0;
        return;
    }
    if (p2 <= 0 || !g_core || !g_core->apply_anomaly_checked) {
        ctx->plugin_storage[kAudit1458Anom + self] = 0;
        return;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), self, p2, ChanceSource::Skill);
    ctx->plugin_storage[kAudit1458Anom + self] = anomaly_landed(r) ? 1 : 0;
}

EffectResult j271_1458_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    j271_1458_round(ctx, args.int_args[0], args.int_args[1], args.int_args[2],
                    args.int_args[3], args.int_args[4]);
    return EffectResult::kOk;
}

// ── 1458 —— {0}回合内每回合{1}%封属，未触发则{2}%令对手{3} ──
// int_args = [owner, target, rounds, p1, p2, anomaly]（args_num=4）。
EffectResult effect_1458_chance_seal_or_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1458, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1458, args,
                     "1458 布局=[owner,target,rounds,p1,p2,异常]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int p1 = args.int_args[3];
    const int p2 = args.int_args[4];
    const int anomaly = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1458,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && p1 >= 0 && p1 <= 100 && p2 >= 0 && p2 <= 100, 1458,
                     args, "需要 回合>0、概率∈[0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1458, args, "异常码越界");
    j271_1458_round(ctx, self, opp, p1, p2, anomaly);   // 首跳：施放当回合
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1458, rounds - 1,
                                    {self, opp, p1, p2, anomaly}, &j271_1458_tick);
    }
    return EffectResult::kOk;
}

// ── 1622 单回合掷点 ──
void j271_1622_round(BattleContext* ctx, int self, int opp, int p1, int times, int pct) {
    const bool sealed = j271_try_seal(ctx, 1622, self, opp, p1);
    ctx->plugin_storage[kAudit1622Arm + self] = sealed ? 0 : 1;
    if (sealed) {
        return;
    }
    if (times <= 0 || pct <= 0) {
        return;
    }
    auto remaining = std::make_shared<int>(times);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, self, DamageEffectCategory::MITIGATE,
        [remaining, pct, self](BattleContext* c, int) {
            if (*remaining <= 0) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;
            }
            d.final = d.final * (100 - pct) / 100;
            --*remaining;
        });
}

EffectResult j271_1622_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    j271_1622_round(ctx, args.int_args[0], args.int_args[1], args.int_args[2],
                    args.int_args[3], args.int_args[4]);
    return EffectResult::kOk;
}

// ── 1622 —— {0}回合内每回合{1}%封属，未触发则下{2}次受伤-{3}% ──
// int_args = [owner, target, rounds, p1, times, pct]（args_num=4）。
EffectResult effect_1622_chance_seal_or_reduce(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1622, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1622, args,
                     "1622 布局=[owner,target,rounds,p1,times,pct]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int p1 = args.int_args[3];
    const int times = args.int_args[4];
    const int pct = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1622,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && p1 >= 0 && p1 <= 100 && times >= 0 && pct >= 0, 1622,
                     args, "需要 回合>0、概率∈[0,100]");
    j271_1622_round(ctx, self, opp, p1, times, pct);   // 首跳
    if (rounds > 1) {
        register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1622, rounds - 1,
                                    {self, opp, p1, times, pct}, &j271_1622_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1458, effect_1458_chance_seal_or_anomaly)
SKILL_EFFECT(1622, effect_1622_chance_seal_or_reduce)
