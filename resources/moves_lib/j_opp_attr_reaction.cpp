// J 组 J283 · 对手属性技反应恢复/增益族 —— 1 宏/8 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   492 [6] "{0}回合内对方使用属性技能，则自身立刻恢复1/{1}生命值且{2}+{3}，{4}+{5}"
//
// 实现口径（单窗 + 事件反应）：
//   - 窗 = EVENT_SKILL_USED watcher（TEAM，duration={0}）：`ev.actor == 对手 &&
//     ev.used_attribute`（载荷 = **声明类型**，见 battleContext 事件注释）→
//     · heal_amount(self, max_hp/{1})（恢复原语：吃封回血/恢复修正，实际量记账）
//     · stat_change(self, kStatMap[{2}], {3})、stat_change(self, kStatMap[{4}], {5})
//       （自身增益走 stat_change 不吃免弱；kStatMap = {0,2,1,3,4,5}，stat 码官方序
//       0攻 1防 2特攻 3特防 4速 5命中 → 引擎能力槽。J48/J81/J275 同表）
//   - 审计槽：991492000+owner = 触发次数；991492100+owner = 最近一次实际回血量；
//     991492300+owner*8+0/1 = 最近一次两项 stat_change 返回值。
//     ⚠️ 2026-10-22 迁段：原 991492200 与 j_drop_rand_anomaly 的
//     `kAuditBase+owner`（landed 种数）同键 → 两边都是审计槽但语义不同，
//     同时触发会互相盖掉（tools/check_storage_keys.py 抓到的真实撞键）。
//   - 行参在位即消费：{1}=0 会除零 → EFFECT_ARG_GUARD 拦（模板分母恒 >0）；
//     {3}/{5}=0 = 该项不变化（仍调用，原语 no-op）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算 8/8 自洽）：
//   492 [rounds, denom, stat1, delta1, stat2, delta2]：
//     17422 八尺勾玉决 [2,3,1,1,3,1]（纯链，场景载体）、18059 神威狱海破 同参、
//     22313 复仇之潮、23722 灵盾守护、23898 固若磐石、24031 天罡霸体、
//     24826 屹立无惧（均 [2,3,1,1,3,1]）、29098 灵动软体 [3,3,2,1,4,1]。
//
// 动工前置三查（2026-10-22）：①内核 grep 492 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch 与
//   各 agents worktree）无 SKILL_EFFECT(492) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6160_opp_attr_reaction.cpp（号段 6160-6169）。

#include "common.h"

namespace {

constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};   // 官方码序 → 引擎能力槽

constexpr int kTriggerAuditBase = 991492000;   // +owner：触发次数
constexpr int kHealAuditBase = 991492100;      // +owner：最近一次实际回血
constexpr int kStatAuditBase = 991492300;      // +owner*8 + 0/1：两项 stat_change 返回值

// ── 492 —— {0}回合内对方使用属性技能 → 自身恢复1/{1}且{2}+{3}、{4}+{5} ──
// int_args = [owner, target, rounds, denom, stat1, delta1, stat2, delta2]（args_num=6）。
EffectResult effect_492_opp_attr_reaction(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 492, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 492, args,
                     "492 布局=[owner,target,回合,分母,槽1,增量1,槽2,增量2]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int stat1 = args.int_args[4];
    const int delta1 = args.int_args[5];
    const int stat2 = args.int_args[6];
    const int delta2 = args.int_args[7];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     492, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 492, args, "窗回合须 > 0");
    EFFECT_ARG_GUARD(denom > 0, 492, args, "分母须 > 0");
    EFFECT_ARG_GUARD(stat1 >= 0 && stat1 <= 5 && stat2 >= 0 && stat2 <= 5,
                     492, args, "能力码须在 [0,5]");
    if (!g_core || !g_core->heal_amount || !g_core->stat_change) {
        return EffectResult::kOk;
    }

    ctx->plugin_storage[kTriggerAuditBase + self] = 0;
    ctx->plugin_storage[kHealAuditBase + self] = 0;
    ctx->plugin_storage[kStatAuditBase + self * 8 + 0] = 0;
    ctx->plugin_storage[kStatAuditBase + self * 8 + 1] = 0;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_USED, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, denom, stat1, delta1, stat2, delta2](BattleContext* c,
                                                         const BattleEvent& ev) {
            if (!c || ev.actor != opp || !ev.used_attribute) {
                return;   // 只认"对手使用属性技能"
            }
            const auto it = c->plugin_storage.find(kTriggerAuditBase + self);
            c->plugin_storage[kTriggerAuditBase + self] =
                (it == c->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;

            const int max_hp = max_hp_of(c, self);
            const int want = max_hp / denom;
            int healed = 0;
            if (want > 0
                && g_core->heal_amount(c, self, want) == HealResult::SUCCESS) {
                healed = c->ws.last_heal_amount[self];   // heal_impl 记录的实际恢复量
            }
            c->plugin_storage[kHealAuditBase + self] = healed;

            const StatChangeResult r1 =
                g_core->stat_change(c, self, kStatMap[stat1], delta1);
            const StatChangeResult r2 =
                g_core->stat_change(c, self, kStatMap[stat2], delta2);
            c->plugin_storage[kStatAuditBase + self * 8 + 0] = static_cast<int>(r1);
            c->plugin_storage[kStatAuditBase + self * 8 + 1] = static_cast<int>(r2);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(492, effect_492_opp_attr_reaction)
