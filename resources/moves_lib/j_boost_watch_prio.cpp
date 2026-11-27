// J79 · 提升监视/先手自升族 —— 1057×15 / 474×15（2026-10-03，worktree ../agents/j79）
//
// 家族语义（effect_info 官方模板 + 载体切片核对）：
//
// 1057（args_num=1）"{0}回合内若自身能力提升状态消失，则消除对手回合类效果"：
//   - 窗口 = {0} 回合 EVENT_STAT_BOOSTS_LOST watcher（E13 TEAM scope，J59 804/998
//     直系——register_watcher 带 rounds，窗口内每次触发独立结算）；
//   - "提升状态消失" = 宽口径：kind=0 消除 / kind=1 吸取都吃（1109 只吃 kind=0 是
//     因其模板明写"被消除"；本条模板只说"消失"→ 两 kind 都算）；
//   - 后果 = break_round_effects(对手)（断回合原语，SUCCESS/NO_EFFECTS/IMMUNE 三态
//     均无后续子句——模板无"成功则"分支，无条件调用）。
//   - 实例：光灵制约 28663 [1057]|[3]（属性技纯链）。
// 474（args_num=3）"先出手时{1}%自身{0}等级+{2}"：
//   - 先手门 = AFTER_ACTION 探针（897 桥形，J12 门框架）：acting==self 且 self 是
//     先手方（preemptive_right 推）→ roll_percent({1})（common.h：0<p<100 消耗 rand、
//     ≥100 必中不消耗）→ stat_change(self, {0}, {2})；
//   - ⚠️ {0} = **引擎槽位直读**（切片 [2,100,1]=防+1、[0,100,1]=攻+1、[4,100,1]=速+1
//     ——单槽参数，无 477 六槽语义序映射问题）。
//   - 实例：怒火梵天绝 14151 [474]|[2,100,1]（特150 纯链 100% 档）。
//
// 动工前置三查（2026-10-03）：①内核 grep 1057/474 无硬编码 ②effect_meta 无分类牵连
//   ③moves_lib 全 worktree 并集无注册——真缺口。切片：side_effect 表 arg_count
//   1057=1/474=3 与 effect_info.args_num 一致；29 载体链切片自洽。
// 先例：J59（j_boost_lost_response.cpp watcher 形）、J16 1109（kind 过滤对照）、
//   J12（门探针框架）、1032（断回合通道）、J69（acting 判定）。
// 场景回归：test/scenario/scenario_1690_boost_watch_prio.cpp（1690 号段）。

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方 + 先手侧（J12/J41 同款小工具）。
int j79_bucket_acting_side(const BattleContext* ctx) {
    if (!ctx) {
        return -1;
    }
    const int first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    switch (ctx->currentState) {
        case State::BATTLE_FIRST_AFTER_ACTION:
        case State::BATTLE_FIRST_ACTION_END:
            return first;
        case State::BATTLE_SECOND_AFTER_ACTION:
        case State::BATTLE_SECOND_ACTION_END:
            return 1 - first;
        default:
            return -1;
    }
}

}  // namespace

// ── 474 门探针（无捕获 EffectFn）：先出手 → roll → 自身槽位+{2} ──
EffectResult j79_474_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int slot = a.int_args[2];
    const int chance = a.int_args[3];
    const int up = a.int_args[4];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (j79_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在自身行动结束判
    }
    const int first = c->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
    if (self != first) {
        return EffectResult::kOk;   // "先出手时"
    }
    if (!roll_percent(c->rng(), chance) || !g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    (void)g_core->stat_change(c, self, slot, up);
    return EffectResult::kOk;
}

// ── 1057 —— {0}回合内若自身能力提升状态消失，则消除对手回合类效果 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_1057_boost_lost_break(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1057, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1057, args,
                     "1057 布局=[owner,target,rounds]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp,
                     1057, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 1057, args, "需要 rounds>0");
    if (!g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    // "消失"宽口径：kind=0 消除 / kind=1 吸取都算（1109 取严是模板明写"被消除"）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_BOOSTS_LOST, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->break_round_effects) {
                return;
            }
            (void)g_core->break_round_effects(c, opp);
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 474 —— 先出手时{1}%自身{0}等级+{2} ──
// int_args = [owner, target, slot, chance, up]（args_num=3）。
EffectResult effect_474_first_mover_self_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 474, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 474, args,
                     "474 布局=[owner,target,slot,chance,up]");
    const int self = args.int_args[0];
    const int slot = args.int_args[2];
    const int chance = args.int_args[3];
    const int up = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 474, args, "owner 越界");
    EFFECT_ARG_GUARD(slot >= 0 && slot < BattleContext::kAbilityLevelSlotCount, 474, args,
                     "槽位越界");
    EFFECT_ARG_GUARD(chance > 0 && up > 0, 474, args, "需要 chance/up > 0");
    if (!g_core || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    const std::vector<int> payload(args.int_args, args.int_args + args.int_count);
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(474, 0, self, -1, EffectArgs(payload), &j79_474_probe);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 474;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

SKILL_EFFECT(1057, effect_1057_boost_lost_break)
SKILL_EFFECT(474, effect_474_first_mover_self_up)
