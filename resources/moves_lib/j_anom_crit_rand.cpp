#include <utils/sbs_rand.h>
// J 组 J113 · 申报制未触发必暴/随机固伤吸取族 —— 1033×13 / 1144×13
//（2026-10-03，worktree ../agents/j113）
//
// 家族语义（effect_info 官方模板即权威）：
//   1033（arg_count=3）"{0}%使对手{1}，未触发则自身下{2}回合攻击必定致命一击"
//   1144（arg_count=2）"附加{0}~{1}点固定伤害并恢复等量体力"
//
// 实现口径：
//   - 1033 = 申报制（apply_anomaly_checked chance={0}，概率申报给原语/亮节闸门）→
//     未落地（anomaly_landed 全档判）→ must_crit NextRounds({2})（crawford 1600
//     未触发必暴同款，ROUND_START tick 写 ws.must_crit）。
//   - 1144 = rand [{0},{1}]（含端点）→ fixed_damage + 按实际扣血 heal_amount
//     （沧岚 drain 形：dealt 回，封回血生效）。
//
// 载体切片（unity 库 json_each 精确匹配 13/13=26；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   1033 [概率,异常码,必暴回合]：分散（纯链 崩坏世纪 31605 / 怒火岩爆 35717）
//   1144 [固低,固高]：分散（纯链 霜冻冰隔 28783 / 利爪重威破 31414）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 1033/1144 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：1600（申报制未触发必暴，crawford）、anomaly_landed（batch_a 判据）、
//   1659（rand 区间+dealt 回，J87）。
// 场景回归：test/scenario/scenario_2070_anom_crit_rand.cpp（2070 号段，一文件两宏）。

#include "common.h"

namespace {

bool j113_anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── 1033 的必暴 tick —— 未触发 → 下 {2} 回合攻击必暴（1600/1204 同通道）──
EffectResult j113_must_crit_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 1) {
        return EffectResult::kOk;
    }
    c->ws.must_crit[a.int_args[0]] = true;
    return EffectResult::kOk;
}


// ── 1033 —— {0}%使对手{1}，未触发则自身下{2}回合攻击必定致命一击 ──
// int_args = [owner, target, chance, anomaly, crit_rounds]（args_num=3）。
EffectResult effect_1033_anomaly_or_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1033, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1033, args,
                     "1033 布局=[owner,target,概率,异常码,必暴回合]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int crit_rounds = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1033, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(chance > 0 && crit_rounds > 0, 1033, args, "需要 概率/必暴回合 > 0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1033, args,
                     "需要 异常码 ∈ [0,43]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
        ctx, opp, anomaly, random_anomaly_duration(), owner, chance,
        ChanceSource::Skill);
    if (j113_anomaly_landed(r)) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1033,
                                crit_rounds, {owner}, &j113_must_crit_tick);
    return EffectResult::kOk;
}

// ── 1144 —— 附加{0}~{1}点固定伤害并恢复等量体力 ──
// int_args = [owner, target, lo, hi]（args_num=2）。
EffectResult effect_1144_rand_fixed_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1144, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1144, args,
                     "1144 布局=[owner,target,固低,固高]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1144, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(lo > 0 && hi >= lo, 1144, args, "需要 区间合法（低>0、高≥低）");
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int amount = choose_skill_range(*ctx, 1144, 0, owner, opp, lo, hi, ChanceSingleton::SkipDraw);
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, amount);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1033, effect_1033_anomaly_or_crit)
SKILL_EFFECT(1144, effect_1144_rand_fixed_drain)
