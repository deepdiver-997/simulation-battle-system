// J 组 J205 · 受物理攻击概率冻伤窗族 —— 1 宏/9 载体（2026-10-22）
//
// 家族语义（effect_info 模板即权威）：
//   92 [2] {0}回合内受到物理攻击时，有{1}%概率将对手冻伤
//
// 实现口径：
//   - EVENT_TAKE_DAMAGE watcher（duration={0}，TEAM 跨回合；716 受击窗同款）：
//     · ev.target==self（打在本效果持有方身上）；
//     · ev.damage_kind==NORMAL（攻击伤害；粉/真/属性直伤不算——E12 口径）；
//     · 物理判定 = `ws.skill_type_view[attacker]==Physical`（实际物化类型；
//       属性直伤已被 NORMAL 过滤，不构成噪声）。
//   - 施加 = apply_anomaly_checked(attacker, Frostbite(5), random_anomaly_duration(),
//     self, {1}, ChanceSource::Skill)——概率**申报给原语**（F7），免疫/抗性/弹控全链生效。
//   - 审计槽 plugin_storage[k92AuditBase+self] = rolled*1000 + landed
//     （ROLL_FAILED→rolled=0；成功/延长/替换/转化→landed=1）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   92 [回合,概率]：23022 冰封之铠 链 [4,4,92] 段 [5,100]（属技，100% 确定性场景载体）、
//        20696 极冰甲 纯链 [5,50]、20875 冰凌镜 [5,50]、21653 冰之领悟 [3,50]…
//        （9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 92 无硬编码 ②custom_effect_overrides/
// programs 无行 ③moves_lib 并集（含 kbatch worktree）无 SKILL_EFFECT(92)——真缺口。
// 场景回归：test/scenario/scenario_5300_phys_hit_frostbite.cpp（号段 5300-5309）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k92AuditBase = 990092100;   // +self：rolled*1000 + landed

// ── 92 —— {0}回合内受到物理攻击时，有{1}%概率将对手冻伤 ──
// int_args = [owner, target, 回合, 概率]（args_num=2）。
EffectResult effect_92_phys_hit_frostbite(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 92, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 92, args,
                     "92 布局=[owner,target,回合,概率]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     92, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0 && chance <= 100, 92, args,
                     "需要 回合>0 且 概率∈(0,100]");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/rounds,
        /*once=*/false,
        [self, chance](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // 只认攻击伤害
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == self) {
                return;
            }
            if (c->ws.skill_type_view[attacker] != static_cast<int>(SkillType::Physical)) {
                return;   // "物理攻击"：实际物化类型为物理（特殊攻击不触发）
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, attacker, static_cast<int>(AbnormalStatusId::Frostbite),
                random_anomaly_duration(), self, chance, ChanceSource::Skill);
            const bool rolled = (r != ApplyAnomalyResult::ROLL_FAILED);
            const bool landed =
                (r == ApplyAnomalyResult::SUCCESS
                 || r == ApplyAnomalyResult::DURATION_EXTENDED
                 || r == ApplyAnomalyResult::REPLACED_EXISTING
                 || r == ApplyAnomalyResult::CONVERTED);
            c->plugin_storage[k92AuditBase + self] =
                (rolled ? 1000 : 0) + (landed ? 1 : 0);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(92, effect_92_phys_hit_frostbite)
