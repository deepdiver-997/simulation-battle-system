// J 组 J274 · 回合效果被消除减伤窗族 —— 1 宏/9 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1886 [3] "{0}回合内若自身回合类效果被消除则对手下{1}回合造成的攻击伤害
//   额外减少{2}%"
//
// 实现口径（直系先例 j_wounded_next_round_reduce.cpp 1584 的"下回合减伤窗"）：
//   - 检测面 = EVENT_BREAK（断回合成功时发；actor=断方、target=被断方）。watcher
//     owner=self、duration={0}，filter ev.target==self（"自身回合类效果被消除"）。
//   - 触发 → register_next_rounds_effect(ROUND_START, rounds={1})（NextRounds 窗界：
//     触发后的下一回合起算，不含触发当回合）→ tick 向 ws.damage_reduce_add[self]
//     首空槽写 +{2}（正值=减伤、加算、4 槽跨槽求和，calculation.h 钳 ±100；
//     红伤专用通道，粉伤天然不受影响）。
//   - 审计槽 991886000+owner = 断回合触发次数；991886100+owner = 最近一次 {2}。
//   - {2} 在位即消费（显式 0 仍注册窗、写 0 等价空槽——不拿 >0 守卫盖掉 0 档）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1886 [rounds, next_rounds, pct]：28141 风轻云淡 [4,1,100]、28181 与夜同眠 [3,2,50]、
//     28439 无终痴念 [5,2,50]（纯链，场景载体）、28676 光耀坚铠 [4,2,50]、
//     28731 极之试炼 [3,2,100]、28974 神鸣 [3,2,50]、28976 神音 [3,2,50]、
//     29277 护身甲阵 [2,1,100]、36993 晓夜占星阵 [3,2,50]（9/9 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 1886 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1886) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6070_break_effect_reduce_window.cpp（号段 6070-6079）。

#include "common.h"

namespace {

constexpr int kBreakBase = 991886000;   // +owner：断回合触发次数
constexpr int kPctBase = 991886100;     // +owner：最近一次减伤百分比

// ── 1886 第二跳：R+1 ROUND_START 写加算减伤槽（首空槽；正值=减伤）──
EffectResult j274_reduce_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int pct = args.int_args[1];
    if (owner < 0 || owner > 1 || pct < 0) {
        return EffectResult::kOk;
    }
    int(&slots)[4] = ctx->ws.damage_reduce_add[owner];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

// ── 1886 —— {0}回合内自身回合效果被消除 → 对手下{1}回合攻击伤害额外-{2}% ──
// int_args = [owner, target, rounds, next_rounds, pct]（args_num=3）。
EffectResult effect_1886_break_effect_reduce_window(BattleContext* ctx,
                                                    const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1886, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1886, args,
                     "1886 布局=[owner,target,回合,下回合数,减伤%]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int next_rounds = args.int_args[3];
    const int pct = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1886, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && next_rounds > 0, 1886, args, "需要 回合 > 0");
    EFFECT_ARG_GUARD(pct >= 0 && pct <= 100, 1886, args, "减伤率须在 [0,100]");

    ctx->plugin_storage[kBreakBase + self] = 0;
    ctx->plugin_storage[kPctBase + self] = pct;
    ctx->event_center_.register_watcher(
        EventType::EVENT_BREAK, self, ctx->roundCount, rounds, /*once=*/false,
        [self, next_rounds, pct](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;   // 只认"自身回合类效果被消除"
            }
            const int prev = any_int_or(c->plugin_storage[kBreakBase + self]);
            c->plugin_storage[kBreakBase + self] = prev + 1;
            c->plugin_storage[kPctBase + self] = pct;
            register_next_rounds_effect(c, State::BATTLE_ROUND_START, self, 1886,
                                        next_rounds, {self, pct}, &j274_reduce_tick);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1886, effect_1886_break_effect_reduce_window)
