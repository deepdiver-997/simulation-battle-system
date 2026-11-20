// J 组 J313 · 降态/异常回合末固伤窗族 + 受击降攻族 —— 2 宏/14 载体（2026-10-23）
//
// 家族语义（effect_info 模板即权威）：
//   439 [2] {0}回合内若自身处于能力下降或异常状态，则对手每回合受到{1}点固定伤害
//   189 [1] {0}回合内若受到攻击，对手攻击等级-1、特攻等级-1
//
// 实现口径：
//   - 439 = 1731 直系（J199 · j_abnormal_endturn_fixed_pp.cpp）：窗 = register_round_effect
//     (BATTLE_ROUND_END, self, {0} 回合)（common.h helper：走 round_effect_start_round 的
//     "N 回合内"口径，持续窗由 ContinuousEffect 自动到期，不手写 plugin_storage 账本——
//     套路目录 §三）；tick 判**自身**（非 1731 的"双方任意一方"）has_any_active_abnormal_status
//     (self) 或 六槽任一负等级（能力下降；能力等级换宠清空，符合"处于"语义）；
//     落点 fixed_damage(opp, {1})（复用 deal_damage，吃护盾/事件）。
//     审计槽 plugin_storage[k439AuditBase+self] = 本窗累计触发回合数（场景可断言"哪几回合
//     真的触发"，无随机）。
//     ⚠️ 模板"每回合"未写"每回合结束后"（1731 写了）——落 ROUND_END tick 与 1731 同款，
//     记档：同一回合内先满足条件则回合末即结算。
//   - 189 = 535 直系（J24 · j_round_window2.cpp）：EVENT_TAKE_DAMAGE watcher（TEAM scope、
//     InRounds {0} 含当回合）；"受攻击"判据同 535：ev.target==self && NORMAL 红伤 &&
//     ev.amount>0 && ev.actor != self（自伤不算）；每次受击都触发（模板无"每回合一次"限定）。
//     落点 stat_drop(攻击方, 槽0 物攻, 1, self) + stat_drop(攻击方, 槽1 特攻, 1, self)
//     ——弱化走 stat_drop 原语（查附加禁令/免弱/可穿强化保护、钳 -6），actor 如实声明 =
//     施术者 self（不是攻击者自己）。能力槽序 0攻 1特攻 2防 3特防 4速 5命中。
//
// 载体切片（side_effect.arg_count 顺序切分；json.loads 后按位切，2026-10-23 脚本核算）：
//   439 [回合,固伤]：21896 森罗万象 纯链 [439]|[5,200]（属性能技，场景载体）、
//        27217 祈愿之星 [3,300]、27421 防御阵型 [4,300]、27871 钢炼雷狱 [4,300]、
//        27793 暗香盈袖 [3,300]、27388 远古守护 [3,300]、23904 天蛇之瞳 [5,200]
//        （7/7 自洽；{0} 回合∈{3,4,5}，{1} 固伤∈{200,300}）
//   189 [回合]：21618 倔强 纯链 [189]|[3]（属性能技，场景载体）、22836 无休刀锋 [3]、
//        23039 高温护甲 [3]、23929 柳暗花明 [3]、24694 能量守护 [3]、23365 无尽黑夜 [2]、
//        23578 战魂之耀 [4]（7/7 自洽；{0} 回合∈{2,3,4}）
//
// 动工前置三查（2026-10-23）：①内核 grep 439/189 无行为硬编码（battleContext.h:1802 的
// "1/2"是语料索引号，非效果 id）②effect_info 模板唯一、custom_effect_programs/overrides
// 无行 ③moves_lib/soul_lib 并集（含 hub 侧未合并 worktree j186/j198/k347）无
// SKILL_EFFECT(439)/SKILL_EFFECT(189) 注册——真缺口。
// 场景回归：test/scenario/scenario_6460_cond_window_damage.cpp（号段 6460-6469）。

#include "common.h"

namespace {

constexpr int k439AuditBase = 990439100;   // +self：本窗累计触发回合数

// 自身是否处于"能力下降或异常状态"（439 门判据；六槽含命中，负等级即能力下降）。
bool j313_self_in_condition(const BattleContext* ctx, int self) {
    if (ctx->has_any_active_abnormal_status(self)) {
        return true;   // 处于异常状态
    }
    for (int slot = 0; slot < BattleContext::kAbilityLevelSlotCount; ++slot) {
        if (ctx->ability_levels[self][slot] < 0) {
            return true;   // 处于能力下降状态
        }
    }
    return false;
}

// ── 439 tick：回合末判定 + 对手固定伤害 ──
// int_args = [owner, target, 固伤]（args_num=2）。
EffectResult j313_439_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || self == opp || amount <= 0) {
        return EffectResult::kOk;
    }
    if (!j313_self_in_condition(ctx, self)) {
        return EffectResult::kOk;   // 门闭：无降态也无异常 → 不结算
    }
    if (g_core && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, amount);
    }
    const auto it = ctx->plugin_storage.find(k439AuditBase + self);
    const int ticks = (it == ctx->plugin_storage.end()) ? 0 : any_int_or(it->second);
    ctx->plugin_storage[k439AuditBase + self] = ticks + 1;
    return EffectResult::kOk;
}

// ── 439 —— {0}回合内若自身处于能力下降或异常状态，则对手每回合受到{1}点固定伤害 ──
// int_args = [owner, target, 回合, 固伤]（args_num=2）。
EffectResult effect_439_self_degraded_round_end_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 439, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 439, args,
                     "439 布局=[owner,target,回合,固伤]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 439, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 439, args, "需要 回合/固伤 > 0");
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 439, rounds,
                          {self, opp, amount}, &j313_439_round_end_tick);
    return EffectResult::kOk;
}

// ── 189 —— {0}回合内若受到攻击，对手攻击等级-1、特攻等级-1 ──
// int_args = [owner, target, 回合]（args_num=1）。
EffectResult effect_189_onhit_attacker_stat_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 189, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 189, args,
                     "189 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     189, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 189, args, "需要 回合 > 0");
    if (!g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->stat_drop) {
                return;
            }
            // "受到攻击" = 红伤（535/817 同判据；自伤不算、粉伤不算）。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            if (ev.actor < 0 || ev.actor > 1 || ev.actor == self) {
                return;
            }
            // 弱化攻击方：物攻槽 0 / 特攻槽 1（actor 如实声明 = 施术者 self）。
            (void)g_core->stat_drop(c, ev.actor, 0, 1, self);
            (void)g_core->stat_drop(c, ev.actor, 1, 1, self);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(439, effect_439_self_degraded_round_end_fixed)
SKILL_EFFECT(189, effect_189_onhit_attacker_stat_drop)
