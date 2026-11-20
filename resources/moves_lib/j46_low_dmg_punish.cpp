// J46 · 低伤罚窗族 —— 742×19（2026-10-02，worktree ../agents/j46）
//
// 742（arg_count=3）"{0}回合内若对手造成的伤害低于{1}，则对手{2}"：
//   - InRounds {0} 回合窗；**对手红伤逐回合累计**（EVENT_TAKE_DAMAGE watcher、
//     target==自身、damage_kind==NORMAL、actor==对手——E12 口径）；
//   - ROUND_END tick（1655 注册面）阈值判（严格低于）→ apply_anomaly_checked
//     (对手, {2}, 申报制 100) → 累计器清零（逐回合独立判定）；
//   - 累计器走 **plugin_storage**（键 = kAccKey+side，J11/994 storage 模式）——
//     watcher 的 std::function 可捕获、但 ROUND_END 的 EffectFn 不可捕获，共享
//     状态落 storage 两边都读得到。
//   - 实例：末世天崩诀 17360 [3,250,6]（全库 19 载体）。
//
// 动工前置三查（2026-10-02）：①内核 grep 742 无行为硬编码 ②effect_meta 无牵连
//   ③moves_lib（本分支）无既有注册——真缺口。载体切片核验 19 载体零异常
//   （[回合,阈值,异常码]）。
// 先例：817（受击 watcher）、775（J15 TAKE_DAMAGE 量判）、J3（申报制）、
//   J11/994（plugin_storage 模式）、1655（ROUND_END 注册面）。
// 场景回归：test/scenario/scenario_1310_low_dmg_punish.cpp（1310 号段，单宏单块）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// 累计器 storage 键（990129xx1+side；994 同款"10^9 段外伪键"）。
constexpr int kAccKeyBase = 990129001;

int j46_accum(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end()) {
        return 0;
    }
    return any_int_or(it->second);
}

// ── 742 ROUND_END tick（无捕获 EffectFn）：阈值判 → 申报异常 → 清零 ──
EffectResult j46_742_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->apply_anomaly_checked || !a.int_args
        || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int threshold = a.int_args[3];
    const int anomaly = a.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int key = kAccKeyBase + self;
    const int dealt = j46_accum(c, key);
    c->plugin_storage[key] = 0;   // 逐回合独立判定（先读后清）
    if (dealt >= threshold) {
        return EffectResult::kOk;   // 严格低于阈值才罚
    }
    (void)g_core->apply_anomaly_checked(c, opp, anomaly, random_anomaly_duration(),
                                        self, 100, ChanceSource::Skill);
    return EffectResult::kOk;
}

// ── 742 —— {0}回合内若对手造成的伤害低于{1}，则对手{2} ──
EffectResult effect_742_low_dmg_punish_window(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 742, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 742, args,
                     "布局=[owner,target,rounds,threshold,anomaly]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int threshold = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1, 742, args,
                     "owner/target 越界");
    EFFECT_ARG_GUARD(rounds > 0 && threshold > 0, 742, args, "需要 rounds>0 && threshold>0");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 742, args, "异常码越界");
    if (!g_core || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int key = kAccKeyBase + self;
    ctx->plugin_storage[key] = 0;
    // 累计：对手红伤（E12 口径）逐次入账。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp, key](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)
                || ev.actor != opp || ev.amount <= 0) {
                return;
            }
            const auto it = c->plugin_storage.find(key);
            const int cur = (it != c->plugin_storage.end())
                                ? any_int_or(it->second)
                                : 0;
            c->plugin_storage[key] = cur + ev.amount;
        },
        WatcherScope::TEAM);
    // 罚则：ROUND_END 每回合判一次（1655 注册面；InRounds {0} 回合）。
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 742, rounds,
                          {self, opp, rounds, threshold, anomaly}, &j46_742_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(742, effect_742_low_dmg_punish_window)
