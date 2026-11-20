// D15 · 洪荒·列奥尼达 4125 双第五技「王·遂古之初 34593 / 荒墟辟易 38247」缺失效果
// 795、1327（夜跑 night-2，2026-09-28）
//
// 34593 链 = [760, 797, 795, 1249]、args [2, 20, 100, 50]：760=[]（已接两处）、
//   797=[2]（selection_seal.cpp 已接）、795=[20, 100]（本文件）、1249=[50]（oracle_skills.cpp 已接）。
// 38247 链 = [699, 697, 1044, 39, 1327]、args [2, 100, 1, 300, 3]：699/697 内核标记、
//   1044=[2]（已接）、39=[100, 1]（已接）、1327=[300, 3]（本文件）。
// 4399 图鉴（王·遂古之初）："必中；攻击时造成的伤害不会出现微弱；消除对手回合类效果，
//   消除成功则2回合内对手无法通过自身技能恢复体力；每次使用威力增加20%，最高增加100%；
//   造成伤害的50%恢复自身体力，若对手处于异常状态则附加等量固定伤害"。荒墟辟易无 4399 文本
//   （新技），1327 按 effect_info 模板落地。
//
// 795 "每次使用则当回合造成的攻击伤害额外提升{0}%，最高额外提升{1}%"：
//   - 递增档写法 = 2396（天衍星霜"每次使用+30%最高+120%"）同款：uses 计数存
//     plugin_storage，SKILL_EFFECT 每用 ws.damage_add_pct += min(uses×step, cap)
//     ——ws 每回合 reset 天然防跨回合叠加；当回合内单次使用只写一次；
//   - ⚠️ 通道口径记档：文本带"额外"字样按规则应走非通用乘法通道，但乘法槽是"占槽
//     叠乘"模型表达不了"累计上限"语义（每用占一槽会叠乘成 1.2^n），故随 2396 先例
//     用通用加法通道；若实测需乘法，翻案点在本 handler 一处。
// 1327 "造成的伤害低于{0}则吸取对手最大体力的1/{1}并为自身附加等量的护盾"：
//   - "伤害低于{0}"在攻击结算后判 → AFTER_ACTION 桥（582 同款）读
//     ws.resolvedDamage.final（管线后，1356 桥同字段）；
//   - 吸取 = deal_pink_damage(PERCENT_VALUE) 打对手 max_hp/denom + heal_amount 等量回自
//     （1249 吸血同款）；等量护盾 = shield_bank_.add（mistike/1754 同款）。
//
// 场景回归：test/scenario/scenario_310_leonidas_4125_795_1327.cpp。

#include "common.h"

namespace {

constexpr int k795UsesKeyBase = 990179510;   // 795 已用次数（owner 侧）
constexpr int k795WatchArmBase = 990179520;  // 795 再上场清零 watcher 防重挂旗（owner 侧）

// ── 795 —— 每次使用则当回合造成的攻击伤害额外提升{0}%，最高额外提升{1}% ──
// int_args = [owner, 视角占位(1), step, cap]——build_effect_args_for_skill 固定前置
// [0,1] 两位（owner 占位 + 持有方视角），模板参数切片从 [2] 起（795 = [20, 100]）。
// ⚠️ 调试记档（2026-09-28）：场景 310 在分支上从未编译通过（make_party 括号错位），
// 合并后首轮回归暴露；排查中曾误判 int_args 布局——真相是只编场景目标不重编
// moves_plugin dylib，旧库里没有 795 handler 而已（模板老教训第三次应验）。
// ⚠️ 累计口径（2026-10-01 人工裁定）：**场上有效、下场失效**——uses 计数在 owner
// 再上场时清零（EVENT_ENTER_STAGE watcher，懒注册一次）。例外备忘：蔑天行·戾杀
// 2472 官方明写"本场战斗"→ 下场保留，落地时走 plugin_storage 常驻，勿套本口径。
EffectResult effect_795_ramp_amp(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 795, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 795, args,
                     "795 布局=[owner,视角占位,step,cap]（int_count>=4）");
    const int owner = args.int_args[0];
    const int step = args.int_args[2];
    const int cap = args.int_args[3];
    if (owner < 0 || owner > 1 || step <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    const int key = k795UsesKeyBase + owner;
    // 懒注册一次：owner **再上场即清计数**（下场失效的等价实现——下场期间无法用技，
    // 只需保证"回到场上时从 0 起"）。ENTER_STAGE 的 actor = 登场方（battleFsm.cpp:626）。
    const int arm_key = k795WatchArmBase + owner;
    if (!ctx->plugin_storage.count(arm_key)) {
        ctx->plugin_storage[arm_key] = 1;
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, owner, ctx->roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [owner](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor != owner) {
                    return;
                }
                wctx->plugin_storage.erase(k795UsesKeyBase + owner);
            },
            WatcherScope::TEAM);
    }
    const auto it = ctx->plugin_storage.find(key);
    const int uses = (it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second)) + 1;
    ctx->plugin_storage[key] = uses;
    ctx->ws.damage_add_pct[owner] += static_cast<float>(std::min(uses * step, cap));
    return EffectResult::kOk;
}

// ── 1327 的 AFTER_ACTION 桥：伤害低于阈值 → 吸 1/denom 最大体力 + 等量护盾 ──
EffectResult effect_1327_drain_shield_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->deal_pink_damage
        || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || threshold <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->ws.resolvedDamage.final >= threshold) {
        return EffectResult::kOk;   // 伤害不低于阈值：不触发
    }
    const int drain = max_hp_of(ctx, target) / denom;
    if (drain <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::PERCENT_VALUE, owner);
    g_core->heal_amount(ctx, owner, drain);
    ctx->getPet(owner).shield_bank_.add(/*priority=*/0, drain, /*source_id=*/1327,
                                        /*duration=*/0);
    return EffectResult::kOk;
}

// ── 1327 —— 造成的伤害低于{0}则吸取对手最大体力的1/{1}并为自身附加等量的护盾 ──
EffectResult effect_1327_low_damage_drain_shield(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || threshold <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    // AFTER_ACTION 桥（582 同款）："伤害低于{0}"在攻击结算后才可知。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1327, 1,
                              {owner, target, threshold, denom},
                              &effect_1327_drain_shield_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(795, effect_795_ramp_amp);
SKILL_EFFECT(1327, effect_1327_low_damage_drain_shield);
