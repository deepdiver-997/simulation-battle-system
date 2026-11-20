// J 组 J284 · 被消强反伤窗族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1474 [2] "{0}回合内若自身能力提升状态被消除，每消除1项则附加{1}点固定伤害"
//
// 实现口径：
//   - 窗 = EVENT_STAT_REMOVED watcher（TEAM，duration={0}）：
//     `ev.target == self`（被消强的是自己）、`ev.actor` 是动手方（0/1，且≠self）。
//   - 载荷 amount = 本次被消除/搬走的**正等级项数**——原语 `clear_stat_boosts_as` /
//     `transfer_stat_boosts_as` 只处理正等级，因此"能力提升被消除"的字面口径由原语
//     自身保证（不会把弱化被清掉也计进来）。
//   - 附加伤害 = amount × {1}，逐事件结算（一次消 3 项 = 3×{1}），走
//     `g_core->fixed_damage(动手方, ...)`；固定伤害原语是 FIXED 粉伤，不会再次触发
//     EVENT_STAT_REMOVED（无递归）。
//   - 审计槽：991474000+owner = 触发次数；991474100+owner = 累计附加固定伤害。
//   - 行参 {1}=0 页显示为 0 点附加：仍然注册窗（在位即消费），原语调用 0 无效但
//     审计与语义保持；不做 `>0` 早退把显式 0 档吞掉。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   26763 烈风雷魂 [3,60]、26809 空间游离 [3,100]、27041 黑瘴附体 [3,80]、
//   27731 耀世光辉 [3,80]、27834 孤潮雷闪 [4,60]、27910 禁雷血祭 [3,80]、
//   28399 安适一隅 [3,80]（场景载体，纯链）、35532 多元时间线 [3,80]。
//
// 动工前置三查（2026-10-22）：①内核 grep 无 1474 行为硬编码 ②effect_info 模板唯一、
//   custom 表零行 ③全库（jbatch/kbatch/各 agents worktree）无 SKILL_EFFECT(1474)
//   注册——真缺口。
//
// 场景回归：test/scenario/scenario_6170_stat_removed_pink.cpp（号段 6170-6179）。

#include "common.h"

namespace {

constexpr int kTriggerAuditBase = 991474000;   // +owner：触发次数
constexpr int kDamageAuditBase = 991474100;    // +owner：累计附加固定伤害

// ── 1474 —— {0}回合内自身能力提升被消除 → 每消除 1 项附加 {1} 点固定伤害 ──
// int_args = [owner, target, rounds, fixed]（args_num=2）。
EffectResult effect_1474_stat_removed_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1474, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1474, args,
                     "1474 布局=[owner,target,窗回合,固定伤害]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int fixed = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1474, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1474, args, "窗回合须 > 0");
    EFFECT_ARG_GUARD(fixed >= 0, 1474, args, "固定伤害须 >= 0");

    ctx->plugin_storage[kTriggerAuditBase + self] = 0;
    ctx->plugin_storage[kDamageAuditBase + self] = 0;

    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_REMOVED, self, ctx->roundCount, rounds, /*once=*/false,
        [self, fixed](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;   // 只认"自身"的能力提升被消除
            }
            if (ev.actor < 0 || ev.actor > 1 || ev.actor == self) {
                return;   // 需要明确的动手方；自己清自己的不算"被消除"
            }
            const int count = ev.amount;
            if (count <= 0) {
                return;
            }
            const auto it = c->plugin_storage.find(kTriggerAuditBase + self);
            c->plugin_storage[kTriggerAuditBase + self] =
                (it == c->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
            const int add = count * fixed;
            const auto id = c->plugin_storage.find(kDamageAuditBase + self);
            c->plugin_storage[kDamageAuditBase + self] =
                (id == c->plugin_storage.end() ? 0 : any_int_or(id->second)) + add;
            if (add > 0 && g_core && g_core->fixed_damage) {
                (void)g_core->fixed_damage(c, ev.actor, add);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1474, effect_1474_stat_removed_pink)
