// J 组 J285 · 击败后下只异常族 —— 2 宏/17 载体（2026-10-22，worktree feat/jbatch）
//
// 家族语义（effect_info 官方模板即权威）：
//   1464 [3] "当回合攻击击败对手时，若对手处于{0}状态则{1}%令对手下1只出场精灵{2}"
//   757  [3] "若对手处于{0}则当回合击败对手时对方下只出战精灵{1}%{2}"
//   两宏同义：击败对手的当回合，若倒地宠处于{0}状态 → {1}% 概率给"下只登场"挂{2}异常。
//   args 顺序 = [状态码, 概率, 异常码]（17/17 载体脚本核算自洽）。
//
// 实现口径（先例：j_defeat_next_entry.cpp 2162 的两段式 watcher；d_fifth_skills 698
// EVENT_DEATH 归因）：
//   - ① EVENT_DEATH（rounds=1，TEAM）：actor==self（最后一刀归因）且 target==opp，
//        且 g_core->has_active_anomaly(opp, {0}) → roll_percent({1}) 成功则武装 pending。
//        状态门在死亡回调里查（击败瞬间口径；不提前到 SKILL_EFFECT，避免同链先手
//        附加异常/被后续兄弟效果清掉造成的时点漂移）。
//   - ② EVENT_ENTER_STAGE（永久，TEAM）：actor=登场方 → actor==opp 时消费 pending，
//        apply_anomaly_checked(100%, ChanceSource::Skill)（概率已在①掷过）。
//   - 审计槽（场景精确断门，9xxxx 私有段）：
//       991464200 + opp = 1464 异常落地(1/0)；990757200 + opp = 757 异常落地(1/0)。
//       门闭（未处于状态 / 概率未中）时审计槽缺席，场景按"缺席"断言。
//   - pending 用 9xxxxx 段独立键：两宏共用同一"下只"槽（单技能载体无冲突；同一
//     施放者带多个不同宏的技能时后注册者覆盖，与 2162 同款记档）。
//
// 载体切片（side_effect.arg_count 顺序切分，2026-10-22 脚本核算，17/17 自洽）：
//   1464 [状态,概率,异常]：33234 极限收割机 [19,50,19]、33568 暗夜喵喵拳 [0,50,0]、
//     33912 黯烬璨光斩 [7,100,7]、34464 炽翼天火倾 [2,100,2]、
//     34667 昼逝夜无尽 [6,100,6]（场景载体）、34828 秘境雪飘零 [5,100,5]、
//     34839 狩猎残杀 [19,50,19]、35428 冰翼寒风流 [15,100,15]、
//     35539 战威圣骑踏 [6,100,6]。
//   757 [状态,概率,异常]：17290 狂狮次元斩 [19,50,19]、18336 暴怒雪狼魂 [15,50,15]、
//     33851 潮涌寒流啸 [5,50,5]、36979 高能焰火爆 [2,80,22]、36984 低温急冻术 [5,80,15]、
//     37258 厉武圣猎斩 [16,100,6]（场景载体）、37841 极冰影闪 [5,100,15]、
//     37901 超重力压拳 [9,100,9]。
//
// 动工前置三查（2026-10-22）：①内核 grep 1464/757 无行为硬编码 ②effect_info 模板唯一、
//   custom_effect_overrides/programs 零行 ③全库 moves_lib/soul_lib 并集（含 kbatch
//   与各 agents worktree）无 SKILL_EFFECT(1464/757) 注册——真缺口。
//
// 场景回归：test/scenario/scenario_6180_defeat_next_anomaly.cpp（号段 6180-6189）。

#include "common.h"

namespace {

constexpr int k1464AuditBase = 991464200;   // +opp：异常落地(1/0)
constexpr int k757AuditBase = 990757200;    // +opp：异常落地(1/0)
constexpr int k1464PendingBase = 991464100; // +opp：待挂异常码（-1/0=无）
constexpr int k757PendingBase = 990757100;  // +opp：待挂异常码（-1/0=无）

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

// 两段式：①本回合击败（且倒地宠处于 state）→ 武装 pending；②对方下只登场 → 挂异常。
void register_defeat_next_anomaly(BattleContext* ctx, int effect_id, int self, int opp, int state,
                                  int chance, int anomaly, int pending_base, int audit_base) {
    // ① 当回合击败探针：actor=最后一刀来源；状态门在死亡回调里查。
    ctx->event_center_.register_watcher(
        EventType::EVENT_DEATH, self, ctx->roundCount, /*rounds=*/1, /*once=*/false,
        [effect_id, self, opp, state, chance, anomaly, pending_base](BattleContext* c,
                                                                  const BattleEvent& ev) {
            if (!c || ev.target != opp || ev.actor != self) {
                return;   // 只认自身最后一刀击败对手在场宠
            }
            if (!g_core || !g_core->has_active_anomaly) {
                return;
            }
            if (!g_core->has_active_anomaly(c, opp, state)) {
                return;   // 状态门：倒地宠不处于{0}状态
            }
            if (!roll_skill_percent(*c, effect_id, 0, self, opp, chance)) {
                return;   // {1}% 未中
            }
            c->plugin_storage[pending_base + opp] = anomaly;
        },
        WatcherScope::TEAM);

    // ② 下只登场：actor=登场方（不是 target）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_ENTER_STAGE, self, ctx->roundCount, /*rounds=*/0, /*once=*/false,
        [self, opp, pending_base, audit_base](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;
            }
            const auto it = c->plugin_storage.find(pending_base + opp);
            if (it == c->plugin_storage.end()) {
                return;   // 无待生效异常
            }
            const int anomaly = any_int_or(it->second, -1);
            c->plugin_storage[pending_base + opp] = 0;   // 只作用于"下只"
            if (anomaly < 0 || !g_core || !g_core->apply_anomaly_checked) {
                return;
            }
            const ApplyAnomalyResult r = g_core->apply_anomaly_checked(
                c, opp, anomaly, random_anomaly_duration(), self, /*chance=*/100,
                ChanceSource::Skill);
            c->plugin_storage[audit_base + opp] = anomaly_landed(r) ? 1 : 0;
        },
        WatcherScope::TEAM);
}

// ── 1464 —— 当回合攻击击败对手时，若对手处于{0}状态则{1}%令下只登场精灵{2} ──
// int_args = [owner, target, 状态码, 概率, 异常码]（args_num=3）。
EffectResult effect_1464_defeat_next_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1464, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1464, args,
                     "1464 布局=[owner,target,状态,概率,异常]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int state = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 1464,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(state), 1464, args, "状态码越界（≤43）");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 1464, args, "异常码越界（≤43）");
    register_defeat_next_anomaly(ctx, 1464, self, opp, state, chance, anomaly,
                                 k1464PendingBase, k1464AuditBase);
    return EffectResult::kOk;
}

// ── 757 —— 若对手处于{0}则当回合击败对手时对方下只出战精灵{1}%{2} ──
// int_args = [owner, target, 状态码, 概率, 异常码]（args_num=3）。与 1464 同义，独立审计段。
EffectResult effect_757_defeat_next_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 757, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 757, args,
                     "757 布局=[owner,target,状态,概率,异常]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int state = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 757,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(state), 757, args, "状态码越界（≤43）");
    EFFECT_ARG_GUARD(is_valid_abnormal_status_id(anomaly), 757, args, "异常码越界（≤43）");
    register_defeat_next_anomaly(ctx, 757, self, opp, state, chance, anomaly,
                                 k757PendingBase, k757AuditBase);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1464, effect_1464_defeat_next_anomaly)
SKILL_EFFECT(757, effect_757_defeat_next_anomaly)
