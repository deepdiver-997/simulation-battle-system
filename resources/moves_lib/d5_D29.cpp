// D29 · 圣灵谱尼 5000 第五技「神灵救世光 31143」缺失效果 1240、1241
//（2026-09-29，分支 feat/d-D29）
//
// 31143 链 = [797, 1240, 1241]、args [3, 1, 3, 200, 2]：797=[3]（selection_seal.cpp 已接）、
//   1240=[1, 3]（本文件）、1241=[200, 2]（本文件）。
// 4399 图鉴（神灵救世光）："必中；消除对手回合类效果，消除成功3回合内对手无法通过自身
//   技能恢复体力；未击败对手则下回合附加自身最大体力1/3的固伤；吸取对手200体力，
//   自身体力低于1/2时吸取效果翻倍"。
//
// 1240 "当回合未击败对手则下{0}次自身技能附加自身最大体力1/{1}的百分比伤害"：
//   - "未击败"在伤害落地后判 → AFTER_ACTION 桥（582 同款，FIRST/SECOND 双侧各一条
//     rounds=1、桶按 owner 过滤只跑出手侧，只活在用技当回合）：对手存活 → 挂
//     charges={0}=1 并记武装回合；
//   - 骑乘 = 常驻 AFTER_ACTION 节点（首次武装时注册一次，rounds=999）：owner 出手回合
//     读 ws.resolvedDamage.final>0（本回合自身攻击伤害确有落地——属性技/被免/全盾
//     无附着点，不消耗次数）→ deal_pink_damage(PERCENT_VALUE, max_hp_of(owner)/{1})
//     并消耗一档。PERCENT_VALUE = "值由来源算出、走百分比抗性/护罩"档（枚举注释即以
//     谱尼系"附加自身已损失体力50%"为原型），1853/774/1709"附加…百分比伤害"同族同落点。
//   - ⚠️ 口径记档①（同回合偏差）：官方模板"下{0}次自身技能"在同回合额外行动（额外行动
//     线）场景应立即附加；本实现以 arm_round 闸门从**下回合**起触发（4399 图鉴口径
//     "下回合附加"）——额外行动场景记已知偏差待拍板。
//   - ⚠️ 口径记档②（刷新不叠加）：武装时已有存档次数 → 刷新为 {0}（不叠加上次未消耗
//     档），arm_round 同步刷新；未消耗场景 = 中间回合只用属性技/被免（final=0 不消耗）。
//
// 1241 "吸取对手{0}点固定体力，自身体力低于1/{1}时吸取效果翻倍"：
//   - SKILL_EFFECT 即时结算（随链；吸取链内结算 1892 同款）：FIXED 粉伤打对手 + 按
//     **血差**回自（"吸取 = 按体力实际变化回"——护罩全吸/免粉/锁血体力没动就不回血，
//     crawford 1892 结果口径，用户 2026-09-20 定稿；回血走 heal_amount 原语，吃封回血）；
//   - 翻倍判据 = hp*{1} < max_hp（**严格低于** 1/{1}，1397 同判，相等不翻倍）。
//
// 场景回归：test/scenario/scenario_450_puni_5000_1240_1241.cpp。

#include "common.h"

namespace {

constexpr int k1240ChargesKeyBase  = 990124010;  // 1240 待触发次数（owner 侧）
constexpr int k1240ArmRoundKeyBase = 990124020;  // 1240 次数武装回合（owner 侧）
constexpr int k1240RiderArmedKeyBase = 990124030; // 1240 骑乘常驻节点已注册旗标（owner 侧）

int storage_int(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second);
}

// ── 1240 骑乘 tick：owner 出手回合有攻击伤害落地 → 附加 1/denom 自身最大体力 ──
EffectResult effect_1240_rider_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1240, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1240, args,
                     "1240 骑乘布局=[owner,denom]（int_count>=2）");
    const int owner = args.int_args[0];
    const int denom = args.int_args[1];
    if (owner < 0 || owner > 1 || denom <= 0 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int charges = storage_int(ctx, k1240ChargesKeyBase + owner);
    if (charges <= 0) {
        return EffectResult::kOk;   // 无待触发次数
    }
    if (ctx->roundCount <= storage_int(ctx, k1240ArmRoundKeyBase + owner)) {
        return EffectResult::kOk;   // 武装当回合不触发（"下1次"，口径记档①）
    }
    if (ctx->ws.resolvedDamage.final <= 0) {
        return EffectResult::kOk;   // 本回合自身攻击伤害未落地：不消耗（属性技/被免/全盾）
    }
    const int target = 1 - owner;
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 对手已倒：无附着点（次数保留）
    }
    const int amount = max_hp_of(ctx, owner) / denom;
    if (amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::PERCENT_VALUE, owner);
    ctx->plugin_storage[k1240ChargesKeyBase + owner] = charges - 1;
    return EffectResult::kOk;
}

// ── 1240 武装 tick（AFTER_ACTION）：当回合未击败对手 → 挂次数 ──
EffectResult effect_1240_arm_tick(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1240, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1240, args,
                     "1240 武装布局=[owner,target,count,denom]（int_count>=4）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int count = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 当回合已击败对手：不武装
    }
    ctx->plugin_storage[k1240ChargesKeyBase + owner] = count;   // 刷新不叠加（口径记档②）
    ctx->plugin_storage[k1240ArmRoundKeyBase + owner] = ctx->roundCount;
    return EffectResult::kOk;
}

// ── 1240 骑乘节点注册（不用 register_round_effect）──
// ⚠️ 效果桶同源去重键 = (source_id << 32) | effect_id，而 register_round_effect 固定
// source_id_ = effect_id → 武装节点（rounds=1，每次用技重挂）会**同键覆盖**骑乘节点
// （2026-09-29 场景 450 TD 抓到：骑乘永不执行）。骑乘只挂一次、无需参与去重，
// 故直接构 ContinuousEffect、source_id_ = 0（timed_bucket：无来源不参与去重）；
// valid_id 绑定照常——被断回合/切换作废与其它回合类效果同口径。
// ⚠️⚠️ Effect 构造第 6 参是 logic 函数指针（默认 nullptr，operator() 对空 logic 静默
// 不执行）——register_round_effect 显式传 fn，手抄注册必须同样传（第二次踩坑，同场景抓到）。
void register_1240_rider(BattleContext* ctx, State trigger, int owner, int denom, EffectFn fn) {
    Effect e(1240, 0, owner, /*rounds=*/999, EffectArgs(std::vector<int>{owner, denom}), fn);
    auto ce = std::make_unique<ContinuousEffect>(
        e, trigger, owner, /*rounds=*/999,
        ctx->round_effect_start_round(owner, 999, EffectWindowKind::InRounds));
    ce->source_id_ = 0;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
}

// ── 1240 —— 当回合未击败对手则下{0}次自身技能附加自身最大体力1/{1}的百分比伤害 ──
EffectResult effect_1240_not_defeat_charge(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1240, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1240, args,
                     "1240 布局=[owner,target,count,denom]（int_count>=4）");
    const int owner = args.int_args[0];
    const int count = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || count <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    // 骑乘常驻节点只注册一次（FIRST/SECOND 双侧各一条；桶按 owner 过滤，只在 owner
    // 自己的 AFTER_ACTION 执行，天然不双发）。
    // ⚠️ 同回合"消耗旧档 + 重新武装"的结算序由键序保证：骑乘 source_id_=0（进程自增
    // 小键）< 武装 (1240<<32)|1240（大键），std::map 升序遍历 → 骑乘先跑（读旧档附加，
    // 场景 450 TG：R3 复用 31143 时 R1 留下的次数按"下1次自身技能"附加在它自己身上），
    // 武装后跑（刷新为新档）。两节点注册顺序对调即坏，勿动。
    if (storage_int(ctx, k1240RiderArmedKeyBase + owner) == 0) {
        ctx->plugin_storage[k1240RiderArmedKeyBase + owner] = 1;
        for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
            register_1240_rider(ctx, st, owner, denom, &effect_1240_rider_tick);
        }
    }
    // 武装桥：每用都重挂（rounds=1，只活在用技当回合的 AFTER_ACTION）。
    const int target = args.int_args[1];
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, owner, 1240, /*rounds=*/1,
                              {owner, target, count, denom}, &effect_1240_arm_tick);
    }
    return EffectResult::kOk;
}

// ── 1241 —— 吸取对手{0}点固定体力，自身体力低于1/{1}时吸取效果翻倍 ──
EffectResult effect_1241_flat_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1241, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1241, args,
                     "1241 布局=[owner,target,amount,denom]（int_count>=4）");
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    const int denom = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0 || denom <= 0
        || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    int drain = amount;
    if (ctx->getPet(owner).hp * denom < max_hp_of(ctx, owner)) {
        drain *= 2;   // 自身体力严格低于 1/denom → 翻倍（1397 同判）
    }
    // 吸取 = 按对手体力**实际变化**回（1892 结果口径）：护罩全吸/免粉/锁血不凭空回血。
    const int hp_before = ctx->getPet(target).hp;
    g_core->deal_pink_damage(ctx, target, drain, DamageKind::FIXED, owner);
    const int received = hp_before - ctx->getPet(target).hp;
    if (received > 0 && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, received);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1240, effect_1240_not_defeat_charge);
SKILL_EFFECT(1241, effect_1241_flat_drain);
