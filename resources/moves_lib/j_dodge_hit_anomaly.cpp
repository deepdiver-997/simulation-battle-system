#include <utils/sbs_rand.h>
// J 组 J272 · 闪避窗命中异常族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1975 [4] "{0}回合内{1}%闪避对手攻击，对手攻击技能命中时随机附加{2}、{3}中的
//   1种异常状态"
//
// 实现口径（两段独立，均按官方字面）：
//   ① 闪避段 = j_prob_anomaly_fallback.cpp 1390 兜底 tick 同款：ws.hit_rate_mod[对手]
//      乘 (1-{1}%)——**当回合直写 + BATTLE_ROUND_START tick 每回合重写**（ws 每回合
//      reset 到 1.0，950 双写纪律）。窗 {0} 回合 = 当回合 + 后续 N-1 次 tick
//      （register_round_effect 的 InRounds 起点为当回合，ROUND_START 当回合已过）。
//   ② 命中段 = EVENT_HIT watcher（e_state_window 477 同款：actor=攻击方、
//      target=防御方；filter ev.target==self）→ 只认**攻击技能**命中
//      （executed_skill_is_attack，属性技的 EVENT_HIT 也算命中，必须过滤）→
//      sbs_rand()%2 抽 {2}/{3} 其一 → apply_anomaly_checked(攻击方, code, 100)
//      （申报制 100%=不消耗掷点）。
//   - 审计槽 991975000 + owner = 本次实际抽中的异常码（-1 = 未触发），场景可无随机断言。
//   - {1} 在位即消费（显式 0 = 不闪避，但命中段照常生效；不拿 >0 守卫盖掉 0 档）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1975 [rounds, dodge, anomalyA, anomalyB]：28317 流水无情 [4,100,31,0]、
//     28636 走风步尘 [2,80,9,28]（场景载体）、28661 烟霞震啸 [3,100,9,6]、
//     29033 二度愚神 [1,100,0,8]、29094 高速威慑 [3,80,5,10]、
//     29203 烟雾覆盖 [3,70,1,20]、29221 黄沙进军 [3,100,16,6]、
//     36785 幽骸蚀影掌 [2,100,7,0]、37463 琉光离散 [2,100,10,20]（9/9 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1975/2274 无行为硬编码 ②effect_info
//   模板唯一、custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集
//   （含 kbatch 与各 agents worktree）无 SKILL_EFFECT(1975) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6050_dodge_hit_anomaly.cpp（号段 6050-6059）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kAuditBase = 991975000;   // +owner：本次抽中的异常码（-1=未触发）

// ── 闪避 tick —— 对手命中率乘 (1-{1}%)；ws 每回合 reset，故每回合重写 ──
// args=[owner, opp, dodge_pct]
EffectResult j272_1975_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int clamped = std::min(pct, 100);
    ctx->ws.hit_rate_mod[opp] *= (1.0f - static_cast<float>(clamped) / 100.0f);
    return EffectResult::kOk;
}

// ── 1975 —— {0}回合内{1}%闪避对手攻击，对手攻击技能命中时随机附加{2}、{3}之一 ──
// int_args = [owner, target, rounds, dodge, anomalyA, anomalyB]（args_num=4）。
EffectResult effect_1975_dodge_hit_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1975, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1975, args,
                     "1975 布局=[owner,target,回合,闪避%,异常A,异常B]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int dodge = args.int_args[3];
    const int anomaly_a = args.int_args[4];
    const int anomaly_b = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1975, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1975, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(dodge >= 0 && dodge <= 100, 1975, args, "闪避率须在 [0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly_a)
                         && is_valid_abnormal_status_id(anomaly_b),
                     1975, args, "异常码越界（≤43）");

    // ① 闪避：当回合直写 + 窗内每回合 ROUND_START 重写（1390 双写口径）。
    std::vector<int> tick_args{self, opp, dodge};
    (void)j272_1975_dodge_tick(ctx, EffectArgs(tick_args));
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1975, rounds,
                          std::move(tick_args), &j272_1975_dodge_tick);

    // ② 命中段：对手攻击技能命中自身 → 随机附加一异常。
    ctx->plugin_storage[kAuditBase + self] = -1;
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_HIT, self, ctx->roundCount, rounds, /*once=*/false,
        [self, anomaly_a, anomaly_b](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->apply_anomaly_checked || ev.target != self) {
                return;
            }
            const int attacker = ev.actor;
            if (attacker < 0 || attacker > 1 || attacker == self) {
                return;
            }
            // EVENT_HIT 属性技能也发（skills.cpp 注释），"攻击技能"须显式过滤。
            if (!executed_skill_is_attack(c, attacker)) {
                return;
            }
            const int code = (sbs_rand() % 2 == 0) ? anomaly_a : anomaly_b;
            c->plugin_storage[kAuditBase + self] = code;
            (void)g_core->apply_anomaly_checked(c, attacker, code, random_anomaly_duration(),
                                                self, 100, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1975, effect_1975_dodge_hit_anomaly)
