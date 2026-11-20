#include <utils/sbs_rand.h>
// J 组 J84 · 下回合反击族 —— 560×15 / 715×15（2026-10-03，worktree ../agents/j84）
//
// 家族语义（effect_info 官方模板即权威）：
//   560（arg_count=1）"下回合其他攻击技能附加{0}点固定伤害"
//   715（arg_count=2）"若下回合受到攻击，则对手随机{0}项能力值-{1}"
//
// 实现口径：
//   - 560 = 956 同形两相去天敌门（J12 直系）：施放 → 当回合双态 AFTER_ACTION
//     武装探针（duration=1，行动方过滤）→ NextRounds(1) 双态 tick；tick：自身真用了技能
//     （used_skill_this_round，J6 口径）且是**攻击技能**（selected_skill_is_attack，
//     common.h——属性技不附加）→ fixed_damage({0})。"其他"=窗口从下回合起算，
//     载体当次天然不在窗内；对手已倒不附加（956 同守卫）。
//   - 715 = EVENT_TAKE_DAMAGE watcher（535/817 通道：TEAM scope；"受攻击"=红伤
//     NORMAL、amount>0、actor=对手）+ **下回合门槛**（lambda 内 roundCount > 施放
//     回合，排除当回合受击）+ **一次性**（plugin_storage 武装旗，触发即清——
//     once=true 会被过滤前事件烧掉，故手动）。后果 = Fisher-Yates 洗牌取前 {0} 槽
//     （1235/batch_b 同款）逐槽 stat_drop {1}（弱化原语查免弱）。
//
// 载体切片（unity 库 json_each 精确匹配 15/15=30；顺序切分 side_effect.arg_count
// 全载体自洽、零剩余，脚本核算 2026-10-03）：
//   560 [固伤]：{150}×5/{200}×3/{100}×3/{250}×2/{120}/{300}
//     （纯链载体 天地乖离剑 17304 [120] 物150）
//   715 [项数,降幅]：{2,2}×7/{3,2}×3/{2,1}×2/{1,3}/{1,2}/{4,-1}
//     （纯链载体 阴冥雷障 35763 [2,1] 物140——2 槽 -1 断言可数）
//
// 动工前置三查（2026-10-03 本 worktree 复核）：①内核 grep 560/715 无硬编码
// ②effect_meta 无行 ③moves_lib/custom overrides 无注册——真缺口。
// 先例：956/920（NextRounds AFTER_ACTION 双态 tick，J12）、535/153（TAKE_DAMAGE
//   watcher 判据，J24）、1441（Fisher-Yates 随机槽，J10/J34）、stat_drop（免弱面）。
// 场景回归：test/scenario/scenario_1740_next_round_counter.cpp（1740 号段，一文件两宏）。

#include <memory>

#include "common.h"

namespace {

// AFTER_ACTION 桶的当前行动方（897/J12/J18 同款小工具，插件各自持有一份）。
int j84_bucket_acting_side(const BattleContext* ctx) {
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

// ── 560 的 NextRounds AFTER_ACTION tick —— 下回合用攻击技能 → 附加 {0} 固伤 ──
EffectResult j84_560_attack_fixed_tick(BattleContext* c, const EffectArgs& a) {
    if (!c || !g_core || !g_core->fixed_damage || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    const int opp = a.int_args[1];
    const int fixed = a.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || fixed <= 0) {
        return EffectResult::kOk;
    }
    // "下回合其他攻击技能"：行动方过滤 + 真用了技能 + 是攻击技（属性技不附加）。
    if (j84_bucket_acting_side(c) != self || !executed_skill_this_round(c, self)
        || !executed_skill_is_attack(c, self)) {
        return EffectResult::kOk;
    }
    if (c->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 对手已被击倒：无附加对象（956 同守卫）
    }
    (void)g_core->fixed_damage(c, opp, fixed);
    return EffectResult::kOk;
}

// ── 560 的武装探针（当回合 AFTER_ACTION、duration=1）：自身行动结束后把
//    NextRounds(1) tick 注册到双态桶——两相形态（956 同款）：若在 SKILL_EFFECT
//    直接注册，当回合 AFTER_ACTION 仍在窗内会当场触发（T1740.a 实证 123=3+120）。
EffectResult j84_560_arm_probe(BattleContext* c, const EffectArgs& a) {
    if (!c || !a.int_args || a.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = a.int_args[0];
    if (j84_bucket_acting_side(c) != self) {
        return EffectResult::kOk;   // 只在自身行动结束后武装一次（双桶择一）
    }
    const std::vector<int> payload{self, 1 - self, a.int_args[2]};
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_next_rounds_effect(c, st, self, 560, /*rounds=*/1, payload,
                                    &j84_560_attack_fixed_tick);
    }
    return EffectResult::kOk;
}

// ── 560 —— 下回合其他攻击技能附加{0}点固定伤害 ──
EffectResult effect_560_next_attack_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 560, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 560, args,
                     "560 布局=[owner,target,固伤]");
    const int self = args.int_args[0];
    const int fixed = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 560, args, "owner 越界");
    EFFECT_ARG_GUARD(fixed > 0, 560, args, "需要 固伤 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect probe(560, 0, self, -1, EffectArgs({self, 1 - self, fixed}),
                     &j84_560_arm_probe);
        auto ce = std::make_unique<ContinuousEffect>(probe, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 560;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

// ── 715 的 TAKE_DAMAGE watcher —— 下回合内受到红伤 → 对手随机 {0} 槽 -{1}（一次性）──
constexpr int k715ArmBase = 990715000;   // +owner：下回合受击武装旗

EffectResult j84_715_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    const int drop = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || owner == opp || slots <= 0
        || drop == 0) {
        return EffectResult::kOk;
    }
    const int drop_abs = drop < 0 ? -drop : drop;   // 降幅=带符号增量约定（惰殆迷思 [4,-1]）
    const int arm_rc = ctx->roundCount;
    ctx->plugin_storage[k715ArmBase + owner] = 1;
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, owner, ctx->roundCount, /*duration_rounds=*/2,
        /*once=*/false,
        [owner, opp, slots, drop_abs, arm_rc](BattleContext* c, const BattleEvent& ev) {
            if (!c || !g_core || !g_core->stat_drop) {
                return;
            }
            const int key = k715ArmBase + owner;
            const auto it = c->plugin_storage.find(key);
            if (it == c->plugin_storage.end() || any_int_or(it->second) != 1) {
                return;   // 已触发过（一次性）
            }
            if (c->roundCount <= arm_rc) {
                return;   // "下回合"：当回合受击不算
            }
            if (ev.target != owner || ev.actor != opp || ev.amount <= 0
                || ev.damage_kind != static_cast<int>(DamageKind::NORMAL)) {
                return;   // "受到攻击"=对手打来的红伤（535/817 同判据）
            }
            c->plugin_storage.erase(it);
            // Fisher-Yates 洗牌取前 {0} 槽（1235/batch_b 同款），逐槽 -{1}（查免弱）。
            int pool[BattleContext::kAbilityLevelSlotCount] = {0, 1, 2, 3, 4, 5};
            const int n = BattleContext::kAbilityLevelSlotCount;
            for (int i = n - 1; i > 0; --i) {
                std::swap(pool[i], pool[sbs_rand() % (i + 1)]);
            }
            for (int k = 0; k < slots && k < n; ++k) {
                (void)g_core->stat_drop(c, opp, pool[k], drop_abs, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ── 715 —— 若下回合受到攻击，则对手随机{0}项能力值-{1} ──
EffectResult effect_715_hit_counter_drop(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 715, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 715, args,
                     "715 布局=[owner,target,项数,降幅]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int slots = args.int_args[2];
    const int drop = args.int_args[3];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     715, args, "需要 owner/target ∈ {0,1}");
    // 降幅=带符号增量约定：惰殆迷思 29454 数据 [4,-1]（fuzz seed 89588 实测）——
    // "随机4项能力值-1"，幅度取 |drop|；0 仍视为数据异常。
    EFFECT_ARG_GUARD(slots > 0 && drop != 0, 715, args, "需要 项数 > 0 且 降幅 ≠ 0");
    return j84_715_arm(ctx, args);
}

}  // namespace

SKILL_EFFECT(560, effect_560_next_attack_fixed)
SKILL_EFFECT(715, effect_715_hit_counter_drop)
