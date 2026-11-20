// E3 · 必暴族批产（2026-09-29，工单：docs_local/docs/05-任务清单/E组批量效果工单.md E3）
//
// 家族成员（effect_info 模板 × args_num）：
//   475（2 参 [threshold, rounds]）"若造成的伤害不足{0}，则下{1}回合的攻击必定致命一击"
//       —— 携带：暗夜雷光击 14118 [300,2]、魔翼狂风裂 14592 [300,1]、毁天灭地诀 17162
//          [300,1]（[482,475,415] 切片中间位）等 50 技。
//   1180（1 参 [rounds]）"吸取对手能力提升状态，吸取成功则自身下{0}回合必定致命一击"
//       —— 携带：破卵而出 31002 [2]、恶念入梦 30810 [2,2]、健步如飞 31168 [2] 等 47 技。
//
// 动工三查（2026-09-29）：①内核无 475/1180 硬编码（skills.cpp/battle_primitives.cpp
// switch 表 + EffectMetaCatalog 均无）；②moves_lib 无既有 SKILL_EFFECT(475/1180)——真缺口。
//
// 落点（家族两成员同一范式）：
// ★ 必暴通道 = 58 圣光气同款：NextRounds 窗口 + BATTLE_ROUND_START tick 每回合重写
//   `ws.must_crit[owner]=true`（ws 每回合 reset → 窗口条目每回合重写；暴击掷点在
//   query_usage ①.5 OR 读该旗标，crit_rate==0 的技能也能"必定"）。为什么不用
//   crit_rate_mod：乘算修正表达不了"必定"（0×任何数=0，round_window.cpp 58 注释）。
//   D26 1395 的 must_crit 口径记档只涉及 BEFORE_SKILL_HIT 同攻击写入——本族是
//   ROUND_START 窗口写、先于攻击掷点，走的是 58 生产通道，无该问题。
// ★ 475 "造成的伤害不足{0}"：伤害落地后才可知 → AFTER_ACTION 桥（582 同款双桶
//   register_once_this_round，miss/被封时 SKILL_EFFECT 整桶不跑 → 桥不注册）。
//   伤害读 `ctx->resolvedDamage.final`（1256/1306 口径："造成的伤害"读管线后最终值；
//   与"造成的攻击伤害"裸伤台账族区分）。
// ★ 1180 "吸取成功" = `transfer_stat_boosts(对手→自身) 返回搬走项数 >0`（1287 同动作
//   原语；对手持 STAT_CLEAR 票时返回 0 = 被挡 = 不给窗口，与"无法被消除或吸取"一致）。
//   转化发生在 SKILL_EFFECT（攻击技的效果段先于伤害管线——官方书写序即如此）。
// ★ "下{N}回合" = NextRounds 窗口（本回合不算，[R+1, R+N]）——582 桥后开窗同款；
//   窗口开在桥 tick（AFTER_ACTION）内，register_next_rounds_effect 自带 NextRounds
//   折算，先/后手都从下回合起算。
//
// 场景回归：test/scenario/scenario_480_must_crit_family_475_1180.cpp。
#include "common.h"

#include <memory>
#include <vector>

namespace {

// ── 必暴窗口 tick（58 同款）：ROUND_START 桶每回合重写 ws.must_crit ──────────────
EffectResult must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

// ── 桥接节点：只在当前回合跑一次（effect_batch8/d5_D7 同款，本地一份）──────────
inline void register_once_this_round(BattleContext* ctx, State trigger, int owner,
                                     int effect_id, std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || !fn) {
        return;
    }
    Effect e(effect_id, 0, owner, /*rounds=*/1, EffectArgs(std::move(ints)), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, /*duration=*/1,
                                                 ctx->roundCount);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
}

// ── 475 —— 若造成的伤害不足{0}，则下{1}回合的攻击必定致命一击 ──────────────────
// Args=[owner, target, threshold, rounds]。桥 tick 读本行动 final 伤害，不足阈值才开窗。
EffectResult effect_475_bridge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int threshold = args.int_args[2];
    const int rounds = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // 伤害达标 → 本条不给
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 475, rounds,
                                {owner, 1 - owner}, &must_crit_tick);
    return EffectResult::kOk;
}

EffectResult effect_475_low_damage_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 475, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 4, 475, args, "需要 [owner,target,threshold,rounds]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 475, args,
                     "owner/target 越界");
    // FIRST/SECOND 各注册一条，只有行动方那条会跑 → 恰好一次（582/532 同款双注册）。
    // 桥载荷必须带全 [owner,target,threshold,rounds]——桥 tick 靠它复判阈值并开窗。
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_once_this_round(ctx, st, owner, 475,
                                 {owner, target, args.int_args[2], args.int_args[3]},
                                 &effect_475_bridge_tick);
    }
    return EffectResult::kOk;
}

// ── 1180 —— 吸取对手能力提升状态，吸取成功则自身下{0}回合必定致命一击 ──────────
// Args=[owner, target, rounds]。吸取 = transfer_stat_boosts 真原语（85/1287 同动作），
// 返回搬走项数，>0 即"吸取成功"；SKILL_EFFECT 时点直接判直接开窗（无需桥）。
EffectResult effect_1180_drain_must_crit(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1180, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 3, 1180, args, "需要 [owner,target,rounds]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1, 1180, args,
                     "owner/target 越界");
    if (rounds <= 0 || !g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/target, /*to=*/owner) <= 0) {
        return EffectResult::kOk;   // 无可吸取或被免消除强化挡下 → 不给窗口
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, owner, 1180, rounds,
                                {owner, 1 - owner}, &must_crit_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(475, effect_475_low_damage_must_crit);
SKILL_EFFECT(1180, effect_1180_drain_must_crit);
