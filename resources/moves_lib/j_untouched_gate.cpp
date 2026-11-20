// J 组 J153 · 未受击门族 —— 2 宏/23 载体（2026-10-04，worktree ../agents/j153）
//
// 家族语义（effect_info 模板即权威；切片脚本核算 2026-10-04）：
//   819  [值类型码,pct] "出手时本回合若未受到攻击伤害，则附加自身{0}值{1}%的百分比
//     伤害，并恢复等量体力" —— 门=本回合未挨过**攻击伤害**（NORMAL 红伤旗）；
//     rider=FIXED stat×{1}%（4399 多载体逐字"固定伤害" > 模板"百分比"字样，
//     1024 判例；恢复等量=按 rider 实际落地 dealt 回，J6 2274 口径）。
//     ⚠️ {0} 值类型码按**语义序**属性（0=攻击、2=特攻、4=速度——暗伺瞬影[2] 4399
//     "特攻值"、异次元神笔[4] 4399 "速度值"双互证）；风暴离子电流[2] 4399 写
//     "攻击力"系文本误差，记档。值读 ws.getTempAbilityValue 视图（吃升降）。
//   1708 [rounds,down] "出手时本回合若未受到伤害则对手下{0}回合先制-{1}"
//     —— 门=本回合未受到**任意**伤害（旗不筛 kind），结算锚点=OLD_ROUND_END_1
//     （整回合口径，见旗机制记档）；尾巴=NextRounds({0}) FIRST_MOVE_RIGHT tick
//     ws.preemptive_level[对手]-={1}（1123/471 通道直系）。
//
// 受击旗机制（J42 1320 置旗直系）：
//   施放时挂**全战场长**（rounds=9999）EVENT_TAKE_DAMAGE 自侧 watcher（TEAM scope）：
//   每次挨打置旗 plugin_storage[kFlagBase + roundCount*2 + side]（任意伤害旗 /
//   仅 NORMAL 红伤旗两套）；门在结算时点读旗（key=当回合）。
//   ⚠️ 记档（首施放盲区）：watcher 自首次施放才置旗——**首次施放回合**的施放前
//   受击不可见（后手先挨打再首施放时门会误开）。后续回合旗齐全；彻底闭环需引擎级
//   per-round 受击台账（battleFsm 共享热点，非夜跑可动，R 组候选）。
//
// 载体切片（unity 库 side_effect.arg_count 切分 12+11=23 全载体自洽、零剩余）：
//   819  [值类型码,pct] ×12：[0,50]×5、[2,50]×3、[4,50]×2、[4,40]×2
//     纯链：板凳毛伏击 32586 [0,50]（物）、暗伺瞬影 33864 [2,50]（特）
//   1708 [rounds,down] ×11：[1,2]×5、[2,2]×3、[2,1]、[3,2]、[1,1]
//     纯链：觉 28296 [1,2]（属）、虚滞 37639 [1,1]（特）
//
// 动工前置三查（2026-10-04 复核工单结论）：①内核 grep 3 id 无行为硬编码
//   ②custom_effect_overrides 零行 ③moves_lib 全 worktree 并集无注册——真缺口。
// 先例：J42 1320（置当回合旗）、J14 1024/1123（NextRounds MOVE_RIGHT 先制 tick）、
//   J6 2274（dealt 回血口径）、判据统一 §四点五（NORMAL=攻击伤害门）。
// 场景回归：test/scenario/scenario_2550_untouched_gate.cpp（号段 2550-2559）。

#include "common.h"

#include <entities/numerical-properties.h>

namespace {

// 旗键：+roundCount*2+side（每回合每方一格；watcher 全战场长持续置旗）。
constexpr int kFlagAtkBase = 990153100;   // 攻击伤害旗（仅 NORMAL 红伤）
constexpr int kFlagAnyBase = 990153200;   // 任意伤害旗

bool j153_flag(const BattleContext* ctx, int base, int side) {
    const auto it = ctx->plugin_storage.find(
        static_cast<unsigned long long>(base + ctx->roundCount * 2 + side));
    return it != ctx->plugin_storage.end() && std::any_cast<int>(it->second) != 0;
}

// 语义序值类型码 → ws 属性（0攻/2特攻/4速；4399 双互证，文件头记档）。
bool j153_stat_code(int code, NumericalPropertyIndex& out) {
    switch (code) {
        case 0: out = NumericalPropertyIndex::PHYSICAL_ATTACK; return true;
        case 2: out = NumericalPropertyIndex::SPECIAL_ATTACK; return true;
        case 4: out = NumericalPropertyIndex::SPEED; return true;
        default: return false;   // 1/3/5（防/特防/体力）全库无实例，记档
    }
}

// 全战场长受击旗 watcher：挨打置任意旗；NORMAL 红伤另置攻击旗。
void j153_arm_flag_watcher(BattleContext* ctx, int self) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, /*start=*/0, /*rounds=*/9999, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self) {
                return;
            }
            const int key = c->roundCount * 2 + self;
            c->plugin_storage[static_cast<unsigned long long>(kFlagAnyBase + key)] = 1;
            if (ev.damage_kind == static_cast<int>(DamageKind::NORMAL)) {
                c->plugin_storage[static_cast<unsigned long long>(kFlagAtkBase + key)] = 1;
            }
        },
        WatcherScope::TEAM);
}

// ── 819 tick —— 出手结算：未受攻击伤害 → rider stat×pct FIXED + dealt 回血 ──
EffectResult j153_819_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core
        || !g_core->fixed_damage || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int stat_code = args.int_args[2];
    const int pct = args.int_args[3];
    const int arm_round = args.int_args[4];
    if (arm_round != ctx->roundCount || j153_flag(ctx, kFlagAtkBase, self)) {
        return EffectResult::kOk;   // 门：本回合已挨过攻击伤害
    }
    NumericalPropertyIndex stat;
    if (!j153_stat_code(stat_code, stat)) {
        return EffectResult::kOk;
    }
    const int value = ctx->ws.getTempAbilityValue(self, stat);
    if (value <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, value * pct / 100);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, self, dealt);   // 恢复等量（按实际落地）
    }
    return EffectResult::kOk;
}

// ── 819 —— 出手时本回合未受攻击伤害 → 附加自身{0}值{1}%伤害+回等量 ──
EffectResult effect_819_untouched_stat_rider(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 819, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 819, args,
                     "819 布局=[owner,target,值类型码,pct]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 819,
                     args, "需要 owner/target ∈ {0,1}");
    NumericalPropertyIndex stat;
    EFFECT_ARG_GUARD(j153_stat_code(args.int_args[2], stat), 819, args, "值类型码越界");
    EFFECT_ARG_GUARD(args.int_args[3] > 0, 819, args, "需要 pct > 0");
    j153_arm_flag_watcher(ctx, self);
    const std::vector<int> payload{self, opp, args.int_args[2], args.int_args[3],
                                   ctx->roundCount};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(819, 0, self, -1, EffectArgs(payload), &j153_819_tick);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 819;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 1708 tick —— MOVE_RIGHT：对手先制 -down（NextRounds 窗内每回合）──
EffectResult j153_1708_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[0];
    const int down = args.int_args[1];
    if (opp < 0 || opp > 1 || down <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[opp] -= down;
    fprintf(stderr, "[DBG-TICK] R=%d pl[%d]=%d\n", ctx->roundCount, opp,
            ctx->ws.preemptive_level[opp]);
    return EffectResult::kOk;
}

// ── 1708 回合末结算 —— 出手回合未受任意伤害 → 武装对手先制降窗 ──
// ⚠️ 锚点=OLD_ROUND_END_1（双侧桶，J6 1839 同款）：TAKE_DAMAGE 事件**晚 drain**——
// 同回合施放前受击在 AFTER_ACTION 时点尚不可见（819 tick 同点却可见=物理/属性攻击
// drain 点不同，白盒）；回合末旗必齐 → 门按"整回合未受伤害"字面判。
EffectResult j153_1708_settle(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int down = args.int_args[3];
    const int arm_round = args.int_args[4];
    if (arm_round != ctx->roundCount || j153_flag(ctx, kFlagAnyBase, self)) {
        return EffectResult::kOk;   // 门：本回合已挨过任意伤害
    }
    fprintf(stderr, "[DBG-ARM] R=%d arm tail rounds=%d down=%d\n", ctx->roundCount, rounds,
            down);
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, opp, 1708, rounds,
                                {opp, down}, &j153_1708_tick);
    return EffectResult::kOk;
}

// ── 1708 —— 出手时本回合未受到伤害 → 对手下{0}回合先制-{1} ──
EffectResult effect_1708_untouched_preempt_down(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1708, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1708, args,
                     "1708 布局=[owner,target,rounds,down]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1708,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(args.int_args[2] > 0 && args.int_args[3] > 0, 1708, args,
                     "需要 回合/降级 > 0");
    j153_arm_flag_watcher(ctx, self);
    const std::vector<int> payload{self, opp, args.int_args[2], args.int_args[3],
                                   ctx->roundCount};
    register_round_effect(ctx, State::BATTLE_OLD_ROUND_END_1, self, 1708, /*rounds=*/1,
                          payload, &j153_1708_settle);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(819, effect_819_untouched_stat_rider);
SKILL_EFFECT(1708, effect_1708_untouched_preempt_down);
