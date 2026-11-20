// J 组 J186 · 护盾/护罩破反馈族二 —— 2 宏/19 载体（2026-10-22，worktree ../agents/j186）
//
// 家族语义（effect_info 模板即权威）：
//   1422 [2] 获得{0}点护罩，护罩消失时恢复自身{1}点体力
//   1661 [3] 获得{0}点护盾，护盾消失时{1}%的概率使对手{2}
//
// 实现口径：
//   - 1422 = J47 1753 的护罩镜像：hood_bank_.add(0,{0})获罩 + 一次性武装旗；
//     EVENT_SHIELD_BROKEN watcher（ev.target==自身）→ 首破 → heal_amount({1}) → 清旗。
//     护罩 = hood_bank_（粉伤吸收；battleContext HOOD 相破罩同样 emit 本事件）。
//   - 1661 = J87 1387 的异常镜像：shield_bank_.add(0,{0})获盾 + 一次性武装旗；
//     首破 → apply_anomaly_checked(对手, {2}, 随机时长, 自身, {1}%, Skill)。
//   - ⚠️ EVENT_SHIELD_BROKEN 载荷只有 (actor,target)、**不带盾种/source 归属**：
//     回调里加一道"本槽仍有剩余盾值 → 破的是另一种"的归属守卫
//     （1422 查 hood_bank_.total()==0、1661 查 shield_bank_.total()==0），
//     避免红盾破触发护罩回血（或护罩破触发护盾异常）。同一来源多笔盾仍按"最后一笔
//     破空时触发"归并（事件粒度所限，记档）。
//   - 一次性旗的作用：技能链每回合重跑会再挂 watcher；旗 = 每份破盾反馈只执行一次，
//     后注册的 watcher 见旗已清即静默（J47/J87 同口径）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   1422 [护罩点,回血]：27074 元宵甜甜 [300,300]、32789 微茫凝天聚 [300,300]、
//        33800 紫晶庇护 链尾 [300,300]…（10/10 自洽）
//   1661 [护盾点,概率,异常]：27731 耀世光辉 [400,100,7]（疲惫）、29429 岩壁建构…
//        （9/9 自洽）
//
// 动工前置三查（2026-10-22）：①内核 grep 1422/1661 无硬编码（1422 仅魂印文档注释）
// ②custom_effect_overrides/programs 无行 ③moves_lib 并集（含 kbatch worktree）无注册
// ——真缺口。
// 场景回归：test/scenario/scenario_5010_shield_break_feedback.cpp（号段 5010-5019）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int k1422ArmedBase = 990142200;   // +owner：护罩破回血一次性旗
constexpr int k1661ArmedBase = 990166100;   // +owner：护盾破异常一次性旗

// ── 1422 —— 获得{0}点护罩，护罩消失时恢复自身{1}点体力 ──
// int_args = [owner, target, 护罩点, 回血]（args_num=2）。
EffectResult effect_1422_hood_break_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1422, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1422, args,
                     "1422 布局=[owner,target,护罩点,回血]");
    const int self = args.int_args[0];
    const int hood = args.int_args[2];
    const int heal = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1422, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(hood > 0 && heal > 0, 1422, args, "需要 护罩点/回血 > 0");
    ctx->getPet(self).hood_bank_.add(/*priority=*/0, hood, /*source_id=*/1422,
                                     /*duration=*/0);
    ctx->plugin_storage[k1422ArmedBase + self] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, heal](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->heal_amount) {
                return;
            }
            const int key = k1422ArmedBase + self;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 旗未置/已消耗
            }
            if (c->getPet(self).hood_bank_.total() > 0) {
                return;   // 破的是护盾（本槽仍有护罩）：事件不带盾种，按槽归属过滤
            }
            c->plugin_storage.erase(key);
            (void)g_core->heal_amount(c, self, heal);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 1661 —— 获得{0}点护盾，护盾消失时{1}%的概率使对手{2} ──
// int_args = [owner, target, 护盾点, 概率, 异常码]（args_num=3）。
EffectResult effect_1661_shield_break_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1661, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1661, args,
                     "1661 布局=[owner,target,护盾点,概率,异常码]");
    const int self = args.int_args[0];
    const int shield = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     1661, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(shield > 0 && chance >= 0 && chance <= 100, 1661, args,
                     "需要 护盾点>0 且 概率∈[0,100]");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1661, args,
                     "1661 {2} 异常码超出 0-43");
    const int opp = 1 - self;
    ctx->getPet(self).shield_bank_.add(/*priority=*/0, shield, /*source_id=*/1661,
                                       /*duration=*/0);
    ctx->plugin_storage[k1661ArmedBase + self] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, self, ctx->roundCount, /*duration_rounds=*/0,
        /*once=*/false,
        [self, opp, chance, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            const int key = k1661ArmedBase + self;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;
            }
            if (c->getPet(self).shield_bank_.total() > 0) {
                return;   // 破的是护罩（本槽仍有护盾）：事件不带盾种，按槽归属过滤
            }
            c->plugin_storage.erase(key);
            (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                                self, chance, ChanceSource::Skill);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1422, effect_1422_hood_break_heal)
SKILL_EFFECT(1661, effect_1661_shield_break_anomaly)
