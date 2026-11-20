// J 组 J303 · 受击阈值异常/固伤兜底窗族 —— 1612×7（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 模板即权威）：
//   1612（args_num=5）"{0}回合内受到的伤害低于{1}时{2}%令对手{3}，
//   未触发则附加{4}点固定伤害"
//
// 实现口径：
//   - 窗口 = NextRounds {0} 回合（施放当回合是开窗动作；T6360.a 锁 R+1..R+N）。
//   - 受击面 = EVENT_TAKE_DAMAGE watcher（挂 self，TEAM scope；j_untouched_gate 1708
//     同款自卡窗）：ev.target==self 且 **ev.amount < {1}**（严格低于；amount<=0 不算
//     "受到伤害"）。
//   - 主干 = 申报制 apply_anomaly_checked(对手, {3}, 随机时长, self, {2}%,
//     ChanceSource::Skill)；"未触发" = 返回 ROLL_FAILED（J209/J210 口径；被免疫/
//     抗性/反弹不是未触发）。{2} 显式 0 = 必不触发 → 直接走兜底（红线 7 在位即消费）。
//   - 兜底 = fixed_damage(对手, {4})（原语；target 是"对手"——模板字面）。
//   - 审计槽：991612100+self = 触发次数；991612200+self = 最近一次 ApplyAnomalyResult
//     （未申报记 -2）；991612300+self = 最近一次兜底固伤。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 核算，7/7 自洽）：
//   **m27296 深渊凝视 [911,1612,1304,843]|[1,3,300,100,23,300,2,20,2,2] →
//   1612=[3,300,100,23,300]（属性技，场景载体；911 自升/1304 穿防/843 先制均良性）** /
//   m27557 奔流鬼面 → [3,300,50,15,300] / m27663 宙之试炼 → [3,300,100,8,300] /
//   m27713 堕光鬼面 → [3,300,100,20,300] / m27830 白霞逐夜 → [3,300,100,5,200] /
//   m28625 铅芯能量 → [3,300,100,6,300] / m36929 异冥纵骸咒 → [2,350,100,30,350]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1612 无行为硬编码（rule_center 注释仅
//   举例"对手处于冻伤状态"旧文，不是本效果）②effect_info 仅本条模板
//   ③moves_lib/soul_lib 并集（jbatch/kbatch 各 worktree）无 SKILL_EFFECT(1612)——真缺口。
//
// 场景回归：test/scenario/scenario_6360_takehit_threshold_anomaly.cpp（号段 6360-6369）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kTriggersAuditBase = 991612100;   // +self：触发次数
constexpr int kResultAuditBase = 991612200;     // +self：最近 ApplyAnomalyResult
constexpr int kFallbackAuditBase = 991612300;   // +self：最近兜底固伤

void j303_set_storage(BattleContext* ctx, int key, int value) {
    ctx->plugin_storage[key] = value;
}

// ── 1612 受击回调体 ──
void j303_on_take_damage(BattleContext* c, const BattleEvent& ev, int self, int threshold,
                         int pct, int anomaly, int fallback, int start, int expire) {
    if (!c || c->roundCount < start || c->roundCount >= expire) {
        return;   // 窗口自卡（NextRounds，起点折算后显式判）
    }
    if (ev.target != self || ev.amount <= 0 || ev.amount >= threshold) {
        return;   // 门：自己受到的伤害**严格低于**阈值
    }
    const int trig = any_int_or(c->plugin_storage[kTriggersAuditBase + self]) + 1;
    j303_set_storage(c, kTriggersAuditBase + self, trig);

    bool triggered = false;
    int result_code = -2;   // -2 = 未申报（pct<=0 / 非法码）
    if (pct > 0 && is_valid_abnormal_status_id(anomaly) && g_core
        && g_core->apply_anomaly_checked) {
        const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
            c, 1 - self, anomaly, random_anomaly_duration(), self, pct, ChanceSource::Skill);
        result_code = static_cast<int>(r);
        triggered = (r != ApplyAnomalyResult::ROLL_FAILED);
    }
    j303_set_storage(c, kResultAuditBase + self, result_code);
    if (triggered) {
        j303_set_storage(c, kFallbackAuditBase + self, 0);
        return;
    }
    if (fallback > 0 && g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(c, 1 - self, fallback);
        j303_set_storage(c, kFallbackAuditBase + self, fallback);
    } else {
        j303_set_storage(c, kFallbackAuditBase + self, 0);
    }
}

}  // namespace

// ── 1612 —— {0}回合内受到伤害低于{1}：{2}%令对手{3}；未触发附加{4}固伤 ──
// int_args = [owner, target, rounds, threshold, pct, anomaly, fallback]（args_num=5）。
EffectResult effect_1612_takehit_threshold_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1612, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 7, 1612, args,
                     "1612 布局=[owner,target,回合,阈值,pct,异常,固伤]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    const int pct = args.int_args[4];
    const int anomaly = args.int_args[5];
    const int fallback = args.int_args[6];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1612,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 1612, args, "需要 回合/阈值 > 0");
    // pct/固伤在位即消费：显式 0 合法（pct=0 必不触发 → 走兜底；固伤=0 → 不给）。
    EFFECT_ARG_GUARD(pct >= 0 && fallback >= 0, 1612, args, "需要 pct/固伤 ≥ 0");
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::NextRounds);
    const int expire = start + rounds;
    // 事件中心不防未来锚点 → 锚当前回合、时长跨到窗末，lambda 自卡窗口（676 同款）。
    const int duration = (start - ctx->roundCount) + rounds;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, duration, /*once=*/false,
        [self, threshold, pct, anomaly, fallback, start, expire](BattleContext* c,
                                                                const BattleEvent& ev) {
            j303_on_take_damage(c, ev, self, threshold, pct, anomaly, fallback, start, expire);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

SKILL_EFFECT(1612, effect_1612_takehit_threshold_anomaly)
