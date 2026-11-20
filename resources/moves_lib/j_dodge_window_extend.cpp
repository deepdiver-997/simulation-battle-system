// J 组 J280 · 闪避窗伤害延长族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1832 [4] "{0}回合内{1}%闪避对手攻击，若本次技能造成的伤害高于{2}则闪避效果延长{3}回合"
//
// 实现口径（两段）：
//   ① 闪避段 = 1975 同款（j_dodge_hit_anomaly.cpp）：`ws.hit_rate_mod[对手]` 乘
//      (1-{1}%)——**当回合直写 + BATTLE_ROUND_START tick 每回合重写**（ws 每回合
//      reset 到 1.0，950 双写纪律）。窗 {0} 回合 = 当回合 + 后续 {0}-1 次 tick。
//   ② 延长段 = 当回合 EVENT_TAKE_DAMAGE **once** watcher（actor==self 且
//      damage_kind==NORMAL = 本技能打出的红伤）：金额 > {2} →
//      `register_next_rounds_effect(ROUND_START, {3})` 追加闪避 tick
//      （NextRounds 起点 = 下一回合，正好是"再延长 {3} 回合"）。
//      "本次技能"= 本回合自己造成的伤害，故 watcher 只活当回合、只触发一次。
//   - 审计槽：991832000+owner = 延长是否触发（1/0）；991832100+owner = 观测到的
//     本次技能红伤（0 = 未观测到/未命中）。
//   - pct 类行参在位即消费：{1}=0 = 不闪避（仍注册延长观察）；{2}=0 = 任何红伤都延长。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   1832 [rounds, dodge, threshold, extend]：35993 冰莹洁光破 [1,100,400,2]（场景载体）、
//     36234 白金千芒闪 [1,100,300,2]、36739 黑胧 [2,70,200,2]、37113 彼空·蔚澜倒影
//     [1,100,300,2]、37768 影月散华 [1,100,400,1]、37783 魔影千重阵 [2,100,300,2]、
//     38187 兰烬蘧栩 [2,100,300,2]、38302 光耀破阵冲 [1,100,300,2]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1832 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与
//   各 agents worktree）无 SKILL_EFFECT(1832) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6130_dodge_window_extend.cpp（号段 6130-6139）。

#include "common.h"

namespace {

constexpr int kExtendAuditBase = 991832000;   // +owner：延长是否触发（1/0）
constexpr int kDamageAuditBase = 991832100;   // +owner：本次技能红伤观测值

// ── 闪避 tick —— 对手命中率乘 (1-{1}%)；ws 每回合 reset，故每回合重写 ──
// args=[owner, opp, dodge_pct]
EffectResult j280_1832_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (opp < 0 || opp > 1 || pct <= 0) {
        return EffectResult::kOk;   // 显式 0 档=不闪避（在位即消费，不注册 tick）
    }
    const int clamped = std::min(pct, 100);
    ctx->ws.hit_rate_mod[opp] *= (1.0f - static_cast<float>(clamped) / 100.0f);
    return EffectResult::kOk;
}

// ── 1832 —— {0}回合内{1}%闪避；本次技能伤害 > {2} 则延长 {3} 回合 ──
// int_args = [owner, target, rounds, dodge, threshold, extend_rounds]（args_num=4）。
EffectResult effect_1832_dodge_window_extend(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1832, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 1832, args,
                     "1832 布局=[owner,target,回合,闪避%,阈值,延长回合]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int dodge = args.int_args[3];
    const int threshold = args.int_args[4];
    const int extend = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1832, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1832, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(dodge >= 0 && dodge <= 100, 1832, args, "闪避率须在 [0,100]");
    EFFECT_ARG_GUARD(threshold >= 0, 1832, args, "阈值须 >= 0");
    EFFECT_ARG_GUARD(extend > 0, 1832, args, "延长回合须 > 0");

    // ① 闪避：当回合直写 + 窗内每回合 ROUND_START 重写（1975 双写口径）。
    std::vector<int> tick_args{self, opp, dodge};
    if (dodge > 0) {
        (void)j280_1832_dodge_tick(ctx, EffectArgs(tick_args));
        register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1832, rounds,
                              std::move(tick_args), &j280_1832_dodge_tick);
    }

    // ② 延长：本回合本技能打出红伤且 > 阈值 → 追加 {3} 回合闪避 tick（NextRounds）。
    ctx->plugin_storage[kExtendAuditBase + self] = 0;
    ctx->plugin_storage[kDamageAuditBase + self] = 0;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, /*duration_rounds=*/1, /*once=*/true,
        [self, opp, dodge, threshold, extend](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != self || ev.target != opp) {
                return;
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // "本次技能造成的伤害"只认红伤（粉伤/真伤不算）
            }
            c->plugin_storage[kDamageAuditBase + self] = ev.amount;
            if (ev.amount <= threshold) {
                return;
            }
            c->plugin_storage[kExtendAuditBase + self] = 1;
            if (dodge <= 0) {
                return;
            }
            std::vector<int> tick_args{self, opp, dodge};
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 1832, extend,
                                        std::move(tick_args), &j280_1832_dodge_tick);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1832, effect_1832_dodge_window_extend)
