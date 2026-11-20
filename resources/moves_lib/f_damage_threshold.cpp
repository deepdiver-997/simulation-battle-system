// F4 · 高低伤判定族批产（2026-09-29，工单：docs_local/docs/05-任务清单/F组批量效果工单.md F4）
//
// 家族成员（effect_info 模板 × args_num，全部 3 参 [threshold, X, Y]）：
//   1106 "当回合造成的攻击伤害若高于{0}则{1}%令对手{2}"
//       —— 携带：耀世飞虎闪 30423（[1106,1029]/[300,100,0 | 300,2,2]）、
//          诡魂绝煞破 30662（[986,1106]/[50 | 280,100,6]）。
//   1066 "造成的伤害低于{0}则附加{1}点固定伤害，若先出手则固定伤害提高{2}点"
//       —— 携带：大智善心明 30134（[779,660,1066]/[0,100 | 300,300,150]）、
//          碎岩无影袭 31513（[1224,1066]/[2 | 300,300,150]）。
//   594 "造成的伤害低于{0}时{1}%令对手{2}"
//       —— 携带：魔帝千裂破 16169（[594,591]/[300,100,6 | 300,1,1]）、
//          苍穹斗转决 16626（[43,591,594]/[5 | 280,2,1 | 280,60,0]）。
//
// 动工三查（2026-09-29）：①内核无 1106/1066/594 硬编码（skills.cpp/battle_primitives.cpp/
// src/effects 均无命中——防 697 式假阳性）；②moves_lib 无既有 SKILL_EFFECT(1106/1066/594)
// ——真缺口；③effect_meta 无穿透分类（本族无"穿透"字样）。
//
// 落点（家族三成员同一范式 = E3 475 AFTER_ACTION 桥）：
// ★ 三条都是"本技能命中后判**自己**造成的伤害"——SKILL_EFFECT 桶时点在 ATTACK_DAMAGE
//   **之前**，当场读不到 final → 必须 AFTER_ACTION 桥（E3 475 同款：本桶注册
//   FIRST/SECOND_AFTER_ACTION 双侧 duration=1 节点，只有行动方那条会跑 → 恰好一次；
//   miss/被封时 SKILL_EFFECT 整桶不跑 → 桥根本不注册）。
// ★ 伤害口径 = `ctx->resolvedDamage.final`（管线后：增伤/减伤/保底/锁伤/挡伤都已作用，
//   1256/1306/475 口径——与"攻击伤害"裸伤台账 ws.raw_attack_damage 族区分）。
//   桥 tick 加 `resolvedDamage.attackerId == owner` 身份门：AFTER_ACTION 只认**自己打的**
//   那笔红伤（防"我用属性技、对手先攻的残留快照"误判——stage_simple_attack_damage 只在
//   攻击尝试时清/写快照，属性技不经过它）。
// ★ 1106"**当回合**造成的攻击伤害"的累计口径：全库查无现役先例（475/1327 同类判据都是
//   单次判定）→ 按**单次判定**实现（本行动 resolvedDamage.final）并记档；若官方实测要求
//   当回合多次攻击累计，翻案点在 effect_1106_bridge_tick 一处。
// ★ 1106/594 异常施加 = apply_anomaly_checked（概率**申报给原语**，2026-09-22 概率闸门
//   管道——掷骰在亮节族闸门改写之后，chance=100 的切片不受闸门影响照常必过）；
//   {2} 是官方异常码（is_valid_abnormal_status_id 校验），回合数 random_anomaly_duration()。
// ★ 1066 附加固伤 = deal_pink_damage(FIXED)（插件造粉伤唯一入口，吃护罩/固定抗性/事件）；
//   "若先出手"在桥 tick 内判 `acting_side_is_first(ctx)`——桥时点在 FIRST/SECOND_AFTER_ACTION，
//   currentState 即出手侧序（先手方的 AFTER_ACTION ∈ [ACTION_START, MOVER_DEATH] 区间）。
//
// 场景回归：test/scenario/scenario_640_damage_threshold_family.cpp。
#include "common.h"

#include <memory>
#include <vector>

namespace {

// ── 桥接节点：只在当前回合跑一次（E3 475/effect_batch8 同款，本地一份）──────────
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

// ── AFTER_ACTION 桥注册（E3 475 同款双桶）：FIRST/SECOND 各一条，只有行动方那条跑 ──
// ⚠️ ints 按值传参逐桶拷贝——循环里 move 会把第二条搬空（FIRST 桶吃 full args、
//    SECOND 桶 int_count=0 的坑，场景 640 T2b 抓到过）。
inline void register_threshold_bridge(BattleContext* ctx, int owner, int effect_id,
                                      std::vector<int> ints, EffectFn fn) {
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_once_this_round(ctx, st, owner, effect_id, ints, fn);
    }
}

// ── 桥 tick 公共守卫：解包 [owner,target,threshold,...] + 身份门 ────────────────
// 返回 false = 参数不合法，或本笔 final 不是 owner 打的（属性技不经过攻击 staging、
// 读到的是对手先攻残留/回合初空快照）→ 不结算。⚠️ 不加 final>0：自己被挡到 0 也是
// "造成的伤害低于{0}"的字面成立档（1066/594 要触发）。
inline bool bridge_gate(BattleContext* ctx, const EffectArgs& args, int& owner, int& target,
                        int& threshold) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return false;
    }
    owner = args.int_args[0];
    target = args.int_args[1];
    threshold = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || threshold <= 0) {
        return false;
    }
    return ctx->resolvedDamage.attackerId == owner;
}

// ── 1106/594 共用桥 tick —— 高于/低于阈值 → {1}% 令对手{2} ──────────────────────
// want_higher=true（1106"高于"）/ false（594"低于"）。概率申报给原语（闸门管辖）。
EffectResult threshold_anomaly_tick(BattleContext* ctx, const EffectArgs& args,
                                    bool want_higher) {
    int owner = 0;
    int target = 0;
    int threshold = 0;
    if (!bridge_gate(ctx, args, owner, target, threshold)) {
        return EffectResult::kOk;
    }
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (!g_core || !g_core->apply_anomaly_checked
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    const int final_damage = ctx->resolvedDamage.final;
    if (want_higher ? (final_damage <= threshold) : (final_damage >= threshold)) {
        return EffectResult::kOk;   // 未跨阈值：本条不给
    }
    (void)g_core->apply_anomaly_checked(ctx, target, anomaly, random_anomaly_duration(),
                                        owner, chance, ChanceSource::Skill);
    return EffectResult::kOk;
}

EffectResult effect_1106_bridge_tick(BattleContext* ctx, const EffectArgs& args) {
    return threshold_anomaly_tick(ctx, args, /*want_higher=*/true);
}

EffectResult effect_594_bridge_tick(BattleContext* ctx, const EffectArgs& args) {
    return threshold_anomaly_tick(ctx, args, /*want_higher=*/false);
}

// ── 1066 桥 tick —— 低于阈值 → 附加{1}点固定伤害；先出手则提高{2}点 ─────────────
EffectResult effect_1066_bridge_tick(BattleContext* ctx, const EffectArgs& args) {
    int owner = 0;
    int target = 0;
    int threshold = 0;
    if (!bridge_gate(ctx, args, owner, target, threshold)) {
        return EffectResult::kOk;
    }
    const int fixed = args.int_args[3];
    const int first_bonus = args.int_args[4];
    if (!g_core || !g_core->deal_pink_damage || fixed <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // 伤害达标：不附加
    }
    // "若先出手"：桥跑在行动方 AFTER_ACTION，currentState 即出手侧（common.h 同判据）
    const int amount = acting_side_is_first(ctx) ? fixed + first_bonus : fixed;
    if (amount > 0) {
        (void)g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── 1106 —— 当回合造成的攻击伤害若高于{0}则{1}%令对手{2} ────────────────────────
// Args=[owner, target, threshold, chance, anomaly]（模板 3 参切片从 [2] 起）。
EffectResult effect_1106_high_damage_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1106, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 1106, args,
                     "1106 布局=[owner,target,threshold,chance,anomaly]（int_count>=5）");
    // ⚠️ 1106"当回合累计"口径按单次判定实现（见文件头记档），翻案点 effect_1106_bridge_tick。
    register_threshold_bridge(ctx, args.int_args[0], 1106,
                              {args.int_args[0], args.int_args[1], args.int_args[2],
                               args.int_args[3], args.int_args[4]},
                              &effect_1106_bridge_tick);
    return EffectResult::kOk;
}

// ── 1066 —— 造成的伤害低于{0}则附加{1}点固定伤害，若先出手则固定伤害提高{2}点 ────
EffectResult effect_1066_low_damage_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1066, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 1066, args,
                     "1066 布局=[owner,target,threshold,fixed,first_bonus]（int_count>=5）");
    register_threshold_bridge(ctx, args.int_args[0], 1066,
                              {args.int_args[0], args.int_args[1], args.int_args[2],
                               args.int_args[3], args.int_args[4]},
                              &effect_1066_bridge_tick);
    return EffectResult::kOk;
}

// ── 594 —— 造成的伤害低于{0}时{1}%令对手{2} ────────────────────────────────────
EffectResult effect_594_low_damage_anomaly(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 594, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_count >= 5, 594, args,
                     "594 布局=[owner,target,threshold,chance,anomaly]（int_count>=5）");
    register_threshold_bridge(ctx, args.int_args[0], 594,
                              {args.int_args[0], args.int_args[1], args.int_args[2],
                               args.int_args[3], args.int_args[4]},
                              &effect_594_bridge_tick);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1106, effect_1106_high_damage_anomaly);
SKILL_EFFECT(1066, effect_1066_low_damage_fixed);
SKILL_EFFECT(594, effect_594_low_damage_anomaly);
