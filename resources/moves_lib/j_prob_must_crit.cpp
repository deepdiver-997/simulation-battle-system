// J 组 J239 · 致命一击概率族 —— 2 宏/19 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1429（args_num=2）"50%的概率打出致命一击，未触发则{0}%令对手{1}"
//   1420（0 参）    "自身处于能力提升状态时50%打出致命一击"
//
// 实现口径（D26 1395 当前攻击必暴直系：SKILL_EFFECT 写 `ctx->crit_happened`）：
//   - 暴击掷点在 ON_SKILL_HIT（早于 SKILL_EFFECT）→ 不能用 ws.must_crit 覆盖本次攻击；
//     D26 1395 的既验通道是**直接置 `ctx->crit_happened[owner] = true`**——ATTACK_DAMAGE
//     阶段只消费该旗标并按 `ws.cached_crit_damage`（默认 200）放大整段并置 isCrit
//     （battleFsm.cpp:693-710）。本族按同一通道落地。
//   - 1429：SKILL_EFFECT 先掷 50%（roll_percent）；命中 → crit_happened=true；
//     未命中 → 再走 apply_anomaly_checked(申报 {0}%，ChanceSource::Skill) +
//     random_anomaly_duration()（"未触发则"= 暴击未触发才轮到异常；红线7：{0}=0
//     在位即消费，不设 >0 门）。
//   - 1420：门 = 自身六槽任一 > 0（"处于能力提升状态"，J51/1103 同判据）；门过再掷 50%。
//   - 审计槽（场景精确断门）：
//       991429100+owner = 暴击 roll 结果（0/1）
//       991429110+owner = ApplyAnomalyResult 整数（-1 = 未走异常分支）
//       991429120+owner = 申报的异常概率 {0}
//       991420100+owner = 1420 门/roll（-1=门闭, 0=roll 未中, 1=暴击）
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1429 [异常概率,官方异常码]：33135 茶点暴击 [50,20]、35741 炽热引燃 [50,22]、
//     35493 幻辰白昼陨 [50,6]、38520 惊人注意力 [100,8]、33516 浊水侵心 [100,6]、
//     37267 狸狸爆发 [100,8]、33683 黎明解封 [100,20]、32882 皓月急袭 [50,20]、
//     35443 逆流之源 [100,6]（9/9 自洽）。
//   1420 []：33281 漫天霜花（纯链）、34013 骨刺重击、35324 极闪电刃、35520 茶点供应、
//     32752 冰狐踏雪袭、33106 绚丽烟花、33511 念力激光、34354 凤鸣映花红、
//     34791 天使之序灭、35649 晴雨迎风舞（10/10 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1429/1420 无硬编码 ②effect_info 模板唯一、
// custom_effect_overrides/programs 无行 ③全库 moves_lib/soul_lib 并集（含各 worktree）
// 无 SKILL_EFFECT(1429/1420)——registry.json moves 集不含两 id，真缺口。
// 场景回归：test/scenario/scenario_5700_prob_must_crit.cpp（号段 5700-5709）。

#include "common.h"

#include <memory>

namespace {

constexpr int k1429RollBase   = 991429100;   // +owner：暴击 roll（0/1）
constexpr int k1429ResultBase = 991429110;   // +owner：ApplyAnomalyResult（-1=未走）
constexpr int k1429PctBase    = 991429120;   // +owner：申报异常概率
constexpr int k1420AuditBase  = 991420100;   // +owner：-1 门闭 / 0 roll 未中 / 1 暴击

// 自身是否有任一正等级（"处于能力提升状态"；J51/1103 同判据）。
bool has_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 1429 —— 50%的概率打出致命一击，未触发则{0}%令对手{1} ──
// int_args = [owner, target, 异常概率, 官方异常码]（args_num=2）。
EffectResult effect_1429_crit_or_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1429, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1429, args,
                     "1429 布局=[owner,target,异常概率,官方异常码]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    const int anomaly = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1429, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1429, args,
                     "需要合法官方异常 id");

    const bool crit = roll_percent(ctx->rng(), 50);
    ctx->plugin_storage[k1429RollBase + owner] = crit ? 1 : 0;
    if (crit) {
        // 当前攻击必暴通道（D26 1395 既验）：掷点已过，直接置落地旗标。
        ctx->crit_happened[owner] = true;
        ctx->plugin_storage[k1429ResultBase + owner] = -1;
        return EffectResult::kOk;
    }
    // 未触发 → 异常分支（申报 {0}%；0 在位即消费）。
    ctx->plugin_storage[k1429PctBase + owner] = pct;
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, target, anomaly, random_anomaly_duration(), owner, pct, ChanceSource::Skill);
    ctx->plugin_storage[k1429ResultBase + owner] = static_cast<int>(r);
    return EffectResult::kOk;
}

// ── 1420 —— 自身处于能力提升状态时50%打出致命一击（0 参） ──
// int_args = [owner, target]。
EffectResult effect_1420_boosted_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1420, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1420, args,
                     "1420 布局=[owner,target]");
    const int owner = args.int_args[0];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1420, args, "需要 owner/target ∈ {0,1}");
    if (!has_boost(ctx, owner)) {
        ctx->plugin_storage[k1420AuditBase + owner] = -1;   // 门闭
        return EffectResult::kOk;
    }
    const bool crit = roll_percent(ctx->rng(), 50);
    ctx->plugin_storage[k1420AuditBase + owner] = crit ? 1 : 0;
    if (crit) {
        ctx->crit_happened[owner] = true;
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1429, effect_1429_crit_or_anomaly)
SKILL_EFFECT(1420, effect_1420_boosted_crit)
