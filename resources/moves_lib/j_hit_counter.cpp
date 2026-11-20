// J117 · 受击反击窗族 —— 642×21（2026-10-03，worktree ../agents/j117）
//
// 家族语义（effect_info 官方模板 + 载体切片 + wiki 档位表核对）：
//
// 642（args_num=3）"{0}回合内若对手攻击技能命中则己方在场精灵{1}%做出{2}反击"：
//   - 窗 = InRounds({0}) EVENT_TAKE_DAMAGE 自侧 watcher（J82 1314/J85 1195 受击响应
//     直系：ev.target==self、NORMAL 红伤、amount>0——红伤只来自攻击技能，即"攻击技
//     命中"门）；含当回合（J4 口径）；
//   - 反击目标 = **攻击者**（ev.actor，非硬编码对手位——受击事件的发起方就是命中
//     己方的攻击技能来源）；
//   - 反击伤害 = 档位表[{2}] 点固伤走 fixed_damage（J2 1170 固伤原语），掷骰
//     roll_percent({1})（非异常概率通道，batch 系口径；≥100 短路必发）；
//   - 档位表（wiki 效果 642 权威，21 载体切片档码全对上）：
//       0轻微=50 / 2强力=150 / 3超强=200 / 4绝对=250 / 5完美=300；码 1 未观测记档。
//
// 动工前置三查（2026-10-03）：①内核 grep 642 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口（scenario_958 的 642 是 **魂印**
//   SOUL_MARK 空间，与本技能效果 642 两个注册空间，无冲突）。切片：side_effect 表
//   arg_count 642=3 与 effect_info.args_num 一致；21 载体切片 [回合,概率,档码]
//   自洽零剩余（脚本核算 2026-10-03）；码表观测 {0,2,3,4,5}，码 1 未观测。
// 先例：J82 1314/J85 1195（TAKE_DAMAGE watcher 事件面）、J2 1170（固伤原语）。
// 场景回归：test/scenario/scenario_2110_hit_counter.cpp（2110 号段）。

#include "common.h"

// ── 642 —— {0}回合内若对手攻击技能命中则己方在场精灵{1}%做出{2}反击 ──
// int_args = [owner, target, rounds, chance, tier]（args_num=3）。
EffectResult effect_642_hit_counter(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 642, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 642, args,
                     "642 布局=[owner,target,rounds,chance,tier]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int tier = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 642, args, "owner 越界");
    EFFECT_ARG_GUARD(rounds > 0 && chance > 0, 642, args, "需要 回合/概率 > 0");
    // 档位表（wiki 权威；码 1 未观测——出现即报参数故障，禁静默跳过）
    int fixed = 0;
    switch (tier) {
        case 0: fixed = 50; break;
        case 2: fixed = 150; break;
        case 3: fixed = 200; break;
        case 4: fixed = 250; break;
        case 5: fixed = 300; break;
        default:
            EFFECT_ARG_GUARD(false, 642, args,
                             "档码 " + std::to_string(tier) + " 不在档位表 {0,2,3,4,5}");
    }
    if (!g_core || !g_core->fixed_damage) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, chance, fixed](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->fixed_damage) {
                return;
            }
            if (ev.actor < 0 || ev.actor > 1 || ev.actor == self) {
                return;   // 反击打"命中己方的攻击者"
            }
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;   // "攻击技能命中" = 红伤
            }
            if (!roll_percent(chance)) {
                return;
            }
            (void)g_core->fixed_damage(c, ev.actor, fixed);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

SKILL_EFFECT(642, effect_642_hit_counter)
