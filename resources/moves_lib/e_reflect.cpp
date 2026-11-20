// E12 · 反伤族 —— 462×44 / 580×41（2026-09-29，worktree ../agents/e12）
//
// 家族语义（"受击回声"，用户定稿 2026-09-29）：挨打时对攻击方回敬伤害。
// 与"挡伤票族"（46 挡N次攻 / 49 挡N点伤 / 816 免伤+真伤附加 / 2292 武道反弹——
// 挡下 + 按挡下量反馈，需内核 BLOCK_DAMAGE ticket，另行立项）不同：462/580 是
// **照常挨打 + 事后回敬**——EVENT_TAKE_DAMAGE watcher 可达（1188 受击窗口同款）。
//
// 内核（随本文件落地）：BattleEvent 补 `damage_kind` 字段（DamageKind as int）——
//   以前三档伤害共用 EVENT_TAKE_DAMAGE，事件里分不出"挨的是不是攻击伤害"
//  （砥砺 37 只能就地判 kind）；两处 emit 点（deal_pink_damage 粉伤路径 /
//   deal_damage 红伤路径）均已带上。
//
// 462（args [rounds, fixed]）"{0}回合内受攻击时反弹{1}点固定伤害"：
//   - 窗口 = {0} 回合**含当回合**（1188 式 duration=rounds，非 NextRounds——
//     "属性技挂盾的当回合起算"官方表现）；
//   - "受攻击"= damage_kind == NORMAL（红伤；粉伤 tick/真伤不触发——字面
//     "受**攻击**"，保守口径记档）；
//   - 反弹 = 对攻击方 deal_pink_damage(FIXED, {1})——吃护罩/抗性/免粉，emit 自身的
//     TAKE_DAMAGE 事件 target=攻击方，不会自激（本 watcher 过滤 target==self）。
//   实例：蒙琪 医疗之心 24606（3回合反弹200）、轮回之心 极光之盾 24347（3回合150）。
// 580（args [rounds, pct]）"{0}回合内自身受到直接攻击伤害的{1}%反馈给对手"：
//   - 同 watcher；回敬量 = 实际扣血 × {1}%（ev.amount = 护盾后实际扣血）。
//   实例：时钟诛机 星海停驻 27796（3回合40%，单效果）。
//
// 前置三查（2026-09-29）：①内核无 462/580 处理 ②effect_meta 无穿透分类
// ③全库无既有实现（1188 是受击上异常、1219 是受伤转自回，均非对敌回敬）。
//
// 场景回归：test/scenario/scenario_580_reflect.cpp。

#include "common.h"

namespace {

// 共用受击窗口 watcher：462/580 只差"回敬量怎么算"。
// ⚠️ WatcherScope 必须 TEAM——ON_STAGE 的 valid_id 判定活不过回合交界
//（E2 的 1219 踩过同款坑）；窗口 duration 覆盖当回合起 {0} 回合。
EffectResult arm_reflect_watcher(BattleContext* ctx, int self, int rounds, int value,
                                 bool by_percent) {
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self, value, by_percent](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || !g_core || !g_core->deal_pink_damage) {
                return;
            }
            // "受攻击" = 红伤（damage_kind 字段，E12 随本文件补齐）；
            // 粉伤 tick（中毒/烧伤结算）/真伤/属性伤害不触发——字面"受攻击"。
            if (ev.damage_kind != static_cast<int>(DamageKind::NORMAL) || ev.amount <= 0) {
                return;
            }
            const int actor = ev.actor;
            if (actor < 0 || actor > 1) {
                return;
            }
            const int back = by_percent ? ev.amount * value / 100 : value;
            if (back > 0) {
                (void)g_core->deal_pink_damage(c, actor, back, DamageKind::FIXED, self);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

EffectResult effect_462_reflect_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int fixed = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || fixed <= 0) {
        return EffectResult::kOk;
    }
    return arm_reflect_watcher(ctx, self, rounds, fixed, /*by_percent=*/false);
}

EffectResult effect_580_reflect_pct(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    return arm_reflect_watcher(ctx, self, rounds, pct, /*by_percent=*/true);
}

}  // namespace

SKILL_EFFECT(462, effect_462_reflect_fixed)
SKILL_EFFECT(580, effect_580_reflect_pct)
