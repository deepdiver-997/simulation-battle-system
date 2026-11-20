// J 组 J206 · 双概率害怕族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   460 [2] {0}%概率令对手害怕，若对手处于能力提升状态则额外附加{1}%概率
//
// 实现口径：
//   - SKILL_EFFECT 直附（攻击技；本族载体均为攻技）。
//   - 基础概率 p={0}；对手六槽（含命中）任一正等级 → **加算** {1}%（封顶 100）。
//     "额外附加{1}%概率"按同一异常的单次掷点合并为 p1+p2（等价于两次独立机会
//     至少中一次，且只施加一次异常）。
//   - 施加走 apply_anomaly_checked(对手, 害怕 6, random_anomaly_duration(), self,
//     chance=100, ChanceSource::Skill)（概率已在上一步申报；免疫/抗性/弹控全链生效）。
//   - 审计槽 plugin_storage[k460AuditBase+self] = rolled*1000 + 本次申报概率 p
//     （场景可精确断言 40/60 两档与落地自洽，不猜随机）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   460 [基础概率,额外概率]：13924 旋风破 纯链 [40,20]（物 80，场景载体）、
//        18115 死亡夺取 [30,20]、33290 暗夜跃扑 [30,20]、34024 腾云战痕 [50,25]…
//        （9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 460 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(460)——真缺口。
// 场景回归：test/scenario/scenario_5320_double_chance_fear.cpp（号段 5320-5329）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k460AuditBase = 990460100;   // +self：rolled*1000 + 申报概率

bool j206_has_boost(const BattleContext* ctx, int side) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// ── 460 —— {0}%概率令对手害怕，若对手处于能力提升状态则额外附加{1}%概率 ──
// int_args = [owner, target, 基础概率, 额外概率]（args_num=2）。
EffectResult effect_460_double_chance_fear(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 460, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 460, args,
                     "460 布局=[owner,target,基础概率,额外概率]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int base_pct = args.int_args[2];
    const int extra_pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     460, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(base_pct >= 0 && base_pct <= 100 && extra_pct >= 0, 460, args,
                     "需要 基础概率∈(0,100] 且 额外概率≥0");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    int pct = base_pct;
    if (j206_has_boost(ctx, opp)) {
        pct = std::min(100, base_pct + extra_pct);   // 对手处于能力提升状态 → 额外附加
    }
    const bool rolled = roll_percent(pct);
    int landed = 0;
    if (rolled) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            ctx, opp, static_cast<int>(AbnormalStatusId::Fear),
            random_anomaly_duration(), self, /*chance_pct=*/100, ChanceSource::Skill);
        landed = (r == ApplyAnomalyResult::SUCCESS
                  || r == ApplyAnomalyResult::DURATION_EXTENDED
                  || r == ApplyAnomalyResult::REPLACED_EXISTING
                  || r == ApplyAnomalyResult::CONVERTED)
                     ? 1
                     : 0;
    }
    ctx->plugin_storage[k460AuditBase + self] = (rolled ? 1000 : 0) + pct;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(460, effect_460_double_chance_fear)
