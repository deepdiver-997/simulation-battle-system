// J 组 J269 · 击败后下只登场族 —— 2 宏/18 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   601  [6] 若本回合击败对手，下回合对方出场精灵{0}
//        {0}=六槽增量，**官方码序 0攻 1防 2特攻 3特防 4速 5命中** →
//        引擎等级槽 kStatMap{0,2,1,3,4,5}（1/2 互换；实际载体非零位只在 4/5，双序同值）。
//   2162 [1] 下次击败对手时，令对方下只出战精灵{0}（{0}=官方异常码）
//
// 实现口径（先例：j_defeat_next_pp_zero.cpp 1663 两段式 TEAM watcher；
// d_fifth_skills.cpp 698 EVENT_DEATH 归因；j_death_relay_chance.cpp 1319 接棒）：
//   - ① EVENT_DEATH：actor==self（最后一刀归因）且 target==opp（对手在场宠阵亡）
//        → 武装 pending。601 的探针 duration_rounds=1（"本回合"）；2162 duration=0
//        且用 consumed 标记实现"下次"（每次施放重置 consumed，见文件末记档）。
//   - ② EVENT_ENTER_STAGE：**actor=登场方**（battleFsm perform_switch 口径）→
//        actor==opp 时消费 pending：
//          601  → 六槽逐项 stat_change(+) / stat_drop(-)；正增益走 stat_change，
//                 负增量走 stat_drop（吃免弱）；
//          2162 → apply_anomaly_checked(100%, ChanceSource::Skill)。
//       消费即清 pending（只作用于"下只"）。
//   - 审计槽：996012200+opp = 601 实际施加的槽数；992162200+opp = 2162 异常落地(1/0)。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算）：
//   601 [6]：18153 毁灭屏障 [0,0,0,0,2,0]（速+2，纯链 + 697）、
//     17436 龙·暮雨天降 [0,0,0,0,0,2]（命中+2）、
//     17253 猩红剧毒刺 [0,0,0,0,1,0]、17867 电•强能爆雷闪 [0,0,0,0,0,2]、
//     18117 幽冥冲刺 [0,0,0,0,1,1]、17884 狂暴怒焰抓 [0,0,0,0,2,0]、
//     34767 荒毒绝命闪 [1,1,1,1,1,1]、37168 光影侵神诀 [1,1,1,1,1,1]…
//   2162 [异常码]：38125 鲨鱼斩舰刀 [16 流血]、38137 断金鳍锋斩 [16]、
//     38246 刹那天迁 [28 束缚]、37392 星光·绝命火焰 [22 焚烬]…（8/8 自洽）。
//
// 动工前置三查（2026-10-22）：①内核 grep 601/2162 无行为硬编码 ②effect_info 模板
//   唯一、custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集
//   （含 kbatch 与各 agents worktree）无 SKILL_EFFECT(601/2162) 注册——真缺口。
//
// 记档：同一施放者若用**不同异常码**的多个 2162 技能，watcher 无法区分施放实例，
//   consumed 标记使先注册者生效（多技能共用一个"下次"槽）；单技能载体无此问题。
//
// 场景回归：test/scenario/scenario_6000_defeat_next_entry.cpp（号段 6000-6009）。

#include "common.h"

#include <vector>

namespace {

// 官方码序（攻,防,特攻,特防,速,命中）→ 引擎等级槽（0攻 1特攻 2防 3特防 4速 5命中）。
constexpr int kStatMap[6] = {0, 2, 1, 3, 4, 5};

constexpr int k601PendingBase = 996010100;   // +opp*10 + i：六槽待生效增量
constexpr int k601ArmedBase = 996010200;     // +opp：pending 已武装
constexpr int k601AuditBase = 996012200;     // +opp：实际施加槽数
constexpr int k2162PendingBase = 992162100;  // +opp：待生效异常码（-1/0=无）
constexpr int k2162ConsumedBase = 992162200; // +self：本次施放的"下次"已消费
constexpr int k2162AuditBase = 992162300;    // +opp：异常落地(1/0)

bool anomaly_landed(ApplyAnomalyResult r) {
    switch (r) {
        case ApplyAnomalyResult::SUCCESS:
        case ApplyAnomalyResult::REPLACED_EXISTING:
        case ApplyAnomalyResult::DURATION_EXTENDED:
        case ApplyAnomalyResult::CONVERTED:
            return true;
        default:
            return false;
    }
}

// ── 601 —— 本回合击败 → 对方下只登场精灵六槽增量 ──
// int_args = [owner, target, d0..d5]（args_num=6）。
EffectResult effect_601_defeat_next_stats(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 601, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 8, 601, args,
                     "601 布局=[owner,target,d0..d5]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 601,
                     args, "需要 owner/target ∈ {0,1}");
    std::vector<int> deltas;
    for (int i = 0; i < 6; ++i) {
        deltas.push_back(args.int_args[2 + i]);
    }

    // ① 本回合击败探针（duration=1；actor=最后一刀来源）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, self, ctx->roundCount, /*rounds=*/1, /*once=*/false,
        [self, opp, deltas](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp || ev.actor != self) {
                return;   // 只认自身最后一刀击败对手在场宠
            }
            for (int i = 0; i < 6; ++i) {
                c->plugin_storage[k601PendingBase + opp * 10 + i] = deltas[i];
            }
            c->plugin_storage[k601ArmedBase + opp] = 1;
        },
        WatcherScope::TEAM);

    // ② 下只登场：actor=登场方（不是 target）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*rounds=*/0, /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;
            }
            if (c->plugin_storage.find(k601ArmedBase + opp) == c->plugin_storage.end()) {
                return;   // 无待生效增量
            }
            std::vector<int> deltas(6, 0);
            for (int i = 0; i < 6; ++i) {
                const auto it = c->plugin_storage.find(k601PendingBase + opp * 10 + i);
                deltas[i] = it == c->plugin_storage.end() ? 0 : any_int_or(it->second);
                c->plugin_storage[k601PendingBase + opp * 10 + i] = 0;
            }
            c->plugin_storage[k601ArmedBase + opp] = 0;   // 只作用于"下只"
            int applied = 0;
            for (int i = 0; i < 6; ++i) {
                const int delta = deltas[i];
                if (delta == 0) {
                    continue;
                }
                const int slot = kStatMap[i];
                if (delta > 0) {
                    if (g_core && g_core->stat_change) {
                        (void)g_core->stat_change(c, opp, slot, delta);
                        ++applied;
                    }
                } else {
                    if (g_core && g_core->stat_drop) {
                        (void)g_core->stat_drop(c, opp, slot, -delta, self);
                        ++applied;
                    }
                }
            }
            c->plugin_storage[k601AuditBase + opp] = applied;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 2162 —— 下次击败 → 对方下只登场精灵{0}异常 ──
// int_args = [owner, target, 异常码]（args_num=1）。
EffectResult effect_2162_defeat_next_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2162, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2162, args,
                     "2162 布局=[owner,target,异常码]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 2162,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 2162, args, "异常码越界");

    // 每次施放重置"本次施放的 next-kill 已消费"标记。
    ctx->plugin_storage[k2162ConsumedBase + self] = 0;

    // ① 持久击败探针：只认自身归因；消费标记防"每次击败都触发"。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, self, /*start=*/0, /*rounds=*/0, /*once=*/false,
        [self, opp, anomaly](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != opp || ev.actor != self) {
                return;
            }
            const auto consumed = c->plugin_storage.find(k2162ConsumedBase + self);
            if (consumed != c->plugin_storage.end() && any_int_or(consumed->second) != 0) {
                return;   // "下次"已用
            }
            c->plugin_storage[k2162ConsumedBase + self] = 1;
            c->plugin_storage[k2162PendingBase + opp] = anomaly;
        },
        WatcherScope::TEAM);

    // ② 下只登场：actor=登场方。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*rounds=*/0, /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;
            }
            const auto it = c->plugin_storage.find(k2162PendingBase + opp);
            if (it == c->plugin_storage.end()) {
                return;
            }
            const int anomaly = any_int_or(it->second, -1);
            c->plugin_storage[k2162PendingBase + opp] = 0;   // 只作用于"下只"
            if (anomaly < 0 || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, /*chance=*/100,
                ChanceSource::Skill);
            c->plugin_storage[k2162AuditBase + opp] = anomaly_landed(r) ? 1 : 0;
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(601, effect_601_defeat_next_stats)
SKILL_EFFECT(2162, effect_2162_defeat_next_anomaly)
