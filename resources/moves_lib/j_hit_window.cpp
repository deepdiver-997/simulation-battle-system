// J 组 J67 · 受击窗族 —— 2 宏/34 载体（2026-10-03，worktree ../agents/j67）
//
// 家族语义（effect_info 模板即权威）：
//   1053 [3] {0}回合内对手使用攻击技能后使对手{1}，未触发则自身恢复最大体力的1/{2}
//   1226 [3] {0}回合内受到攻击伤害降低{1}点，回合结束时若当回合未受到攻击伤害则吸取对手{2}点体力
//
// 实现口径：
//   - 1053 = ⚠️ 归位 ROUND_END 窗（记档：对手 NONE/被控回合**不跑其 AFTER_ACTION 桶**
//     ——实测 R2 对手 NONE 时回血分支永不执行，703 对手侧窗方案在此族不成立）→
//     register_round_effect(State::BATTLE_ROUND_END, owner, rounds)（双侧执行、本人名下
//     每回合一次），tick 判 used_skill_this_round(opp)+selected_skill_is_attack(opp)：
//     真用了攻击技 → 申报制异常 apply_anomaly_checked({1})（J3 口径）；否则 →
//     heal_max_hp_fraction(self, 1/{2})（1290 口径）。触发时点较官方"攻击后"晚到回合末
//     （同回合语义不变，记档）。
//   - 1226 = 红伤扁平减：REDUCE_FLAT 管线回调（1717 同款 attackerId/防御侧判定 +
//     回合界 lambda 内判 [r0, r0+{0}-1]），"受到攻击伤害"旗标写 plugin_storage
//     （回调是 std::function 可捕获、ROUND_END tick 是裸 EffectFn 不可捕获 →
//     经 storage 传旗标）；回合末 State::BATTLE_ROUND_END 窗 tick（双侧执行、本人
//     名下条目每回合一次）读旗标：未受击 → 吸取对手{2}点（drain_hp_amount 未暴露
//     → fixed_damage 落地后按实际扣血 heal_amount 回，canglan 先例），并清旗标跨回合。
//
// plugin_storage 键段（990167x，grep 全库无撞）：
//   990167100 + owner   1226 本回合已受攻击伤害旗标（1/0，回合末清）
//
// 载体切片（unity 库 json_each 精确匹配 17+17 计账；两族全属性技载体，无纯链——
// 场景走"兄弟全实现且良性"链，防链激活翻车）：
//   1053 [回合,异常码,分母]：26699 [48,1053,843]|[3,3,6,3,2,1]（48=免疫异常/843=先制，良性）
//   1226 [回合,减量,吸取点]：27619 [48,1226,861]|[4,3,300,300,4,2]（48/861=属性技PP减，良性）
//
// 动工前置三查（2026-10-03）：①内核 grep 1053/1226 无硬编码 ②
// custom_effect_overrides 无声明 ③moves_lib 并集无既有注册——真缺口。
// 场景回归：test/scenario/scenario_1540_hit_window.cpp（号段 1540-1549）。

#include "common.h"

#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kActionSelectSkill = 1;        // = BattleFsm::ActionType::SELECT_SKILL
constexpr int k1226HitKeyBase = 990167100;   // +owner：本回合已受攻击伤害旗标
constexpr int k1226ArmKeyBase = 990167200;   // +owner：1226 管线回调已武装旗标（防叠注册）
constexpr int k1053LastSeqBase = 990167300;  // +owner：1053 对手 skill_use_seq 上次检查值

int j67_stor(const BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1053 的回合末 tick —— payload={owner, opp, anomaly, denom} ──
EffectResult j67_1053_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->apply_anomaly_checked) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int anomaly = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || denom <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // ★ "成功使用"判据（用户 2026-10-03 裁定：使用=成功使用出来，被控不算）：
    //   skill_use_seq 在执行主流程 ++（被控/嗑药/NONE 不动；miss/被盔算使用）→
    //   本回合 seq 较上次检查有增量 = 成功使用了技能；roundChoice 只记选择
    //   （被控也写 SELECT_SKILL），仅在 seq 有增量时用于判技能类型。
    //   last_seen 存 plugin_storage 跨回合（注册时以施放点为基线：对手先于施放的
    //   行动不计入窗口）。
    const int seq_key = k1053LastSeqBase + owner;
    const int cur_seq = ctx->skill_use_seq[opp];
    const int last_seen = j67_stor(ctx, seq_key);
    ctx->plugin_storage[seq_key] = cur_seq;
    if (cur_seq > last_seen && executed_skill_is_attack(ctx, opp)) {
        (void)g_core->apply_anomaly_checked(ctx, opp, anomaly, random_anomaly_duration(), owner,
                                            100, ChanceSource::Skill);
        return EffectResult::kOk;   // 触发：不回血
    }
    (void)heal_max_hp_fraction(ctx, owner, denom);   // 未触发 → 自身回 1/{2}
    return EffectResult::kOk;
}

// ── 1053 —— {0}回合内对手使用攻击技能后使对手{1}，未触发则自身恢复最大体力的1/{2} ──
// int_args = [owner, target, rounds, anomaly, denom]（args_num=3）。
EffectResult effect_1053_foe_attack_watch(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1053, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1053, args,
                     "1053 布局=[owner,target,回合,异常码,分母]");
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && opp >= 0 && opp <= 1 && owner != opp,
                     1053, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && is_valid_abnormal_status_id(anomaly), 1053,
                     args, "需要 回合/分母>0 且合法异常码");
    // 基线 = 施放时对手 seq（对手先于施放的行动不计入窗口；last_seen 由 tick 逐回合推进）
    ctx->plugin_storage[k1053LastSeqBase + owner] = ctx->skill_use_seq[opp];
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1053, rounds,
                          {owner, opp, anomaly, denom}, &j67_1053_tick);
    return EffectResult::kOk;
}

// ── 1226 的回合末 tick —— payload={owner, target, drain, r0, rounds} ──
EffectResult j67_1226_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->fixed_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int drain = args.int_args[2];
    const int r0 = args.int_args[3];
    const int rounds = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || drain <= 0 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->roundCount < r0 || ctx->roundCount > r0 + rounds - 1) {
        return EffectResult::kOk;   // 窗口界
    }
    const int hit_key = k1226HitKeyBase + owner;
    if (j67_stor(ctx, hit_key) == 0 && ctx->getPet(target).hp > 0) {
        // "吸取"（drain_hp_amount 未暴露）：固伤落地后按实际扣血回（canglan 先例）
        const int before = ctx->getPet(target).hp;
        g_core->fixed_damage(ctx, target, drain);
        const int dealt = before - ctx->getPet(target).hp;
        if (dealt > 0) {
            (void)g_core->heal_amount(ctx, owner, dealt);
        }
    }
    ctx->plugin_storage[hit_key] = 0;   // 旗标跨回合清零
    return EffectResult::kOk;
}

// ── 1226 —— {0}回合内受到攻击伤害降低{1}点，回合末未受击→吸取对手{2}点 ──
// int_args = [owner, target, rounds, flat, drain]（args_num=3）。
EffectResult effect_1226_flat_reduce_not_hit_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1226, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1226, args,
                     "1226 布局=[owner,target,回合,减量,吸取点]");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int flat = args.int_args[3];
    const int drain = args.int_args[4];
    EFFECT_ARG_GUARD(owner >= 0 && owner <= 1 && target >= 0 && target <= 1 && owner != target,
                     1226, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && flat > 0 && drain > 0, 1226, args,
                     "需要 回合/减量/吸取点 > 0");
    // ⚠️ 技能链每回合重跑 SKILL_EFFECT → 回调/窗条目会叠注册（实测 R3 减 900=3×300）
    // → 武装旗标只注册一次，窗口以首用起算。
    const int arm_key = k1226ArmKeyBase + owner;
    if (j67_stor(ctx, arm_key) != 0) {
        return EffectResult::kOk;
    }
    ctx->plugin_storage[arm_key] = 1;
    // 扁平减（REDUCE_FLAT，1717 同款回调）：窗口 [r0, r0+{0}-1]，受击置旗标
    const int r0 = ctx->roundCount;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, owner, DamageEffectCategory::MITIGATE,
        [owner, flat, r0, rounds](BattleContext* c, int) {
            if (!c || c->roundCount < r0 || c->roundCount > r0 + rounds - 1) {
                return;   // 窗口界
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != owner || !d.isRed) {
                return;   // 只减打向自身的红伤
            }
            if (d.final > 0) {
                c->plugin_storage[k1226HitKeyBase + owner] = 1;   // 受击旗标
            }
            d.final = std::max(0, d.final - flat);
        });
    // 回合末未受击检测（State::BATTLE_ROUND_END 双侧执行，本人名下每回合一次）
    register_round_effect(ctx, State::BATTLE_ROUND_END, owner, 1226, rounds,
                          {owner, target, drain, r0, rounds}, &j67_1226_round_end_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1053, effect_1053_foe_attack_watch)
SKILL_EFFECT(1226, effect_1226_flat_reduce_not_hit_drain)
