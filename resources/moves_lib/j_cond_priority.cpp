// J 组 J53 · 条件先制族 —— 454×19 / 1484×17 / 1487×16（2026-10-02，worktree ../agents/j53）
//
// 家族语义（effect_info 官方模板即权威），三条都是"条件成立 → 本技能先制+N"：
//   454（arg_count=2）"当自身血量少于1/{0}时先制+{1}（boss有效）"
//   1484（arg_count=0）"体力低于最大体力的1/2时先制+1（boss有效）"
//   1487（arg_count=0）"自身处于护盾状态下先制+1（boss有效）"
//
// 实现口径（2298/779/1860 选择期路由直系，J11/J29 先例）：
//   - skills.cpp 路由表 2026-10-02 加 454/1484/1487 三行——点击即判定、先手权
//     比较前生效；用技后注册的 MOVE_RIGHT 节点首回合吃不到加成，选择期路由
//     才是正位（J11 注）。零参布局同 1860：args[0/1] 由 on_selected 绑定
//     owner/对手；454 带参布局同 881：[owner, target, 除数, 先制量]。
//   - 判据：
//     · 454 低体 = hp×{0} < max_hp（crawford 1397"严格低于"口径，整数直乘
//       免除法截断）→ preemptive_level[自身] += {1}；
//     · 1484 = 454 的零参定档（denom=2、bonus=1），1860 零参形；
//     · 1487 = shield_bank_.total() > 0（J26 双账口径：护盾账吃红伤，
//       "护盾状态"即护盾账非空；护罩是另一本账不算——J53 工单裁定）。
//
// 载体切片（unity 库 json_each 精确匹配 19/17/16=52；顺序切分 side_effect.arg_count
// 全载体自洽，脚本核算 2026-10-02 零异常：454 切片=[除数,先制量]，[3,1]×14 为主、
// [2,2]×3（屠戮之刃等）、[3,2]×1（雷鸣天荒闪）；1484/1487 全部空参）。
//
// 动工前置三查（2026-10-02 本 worktree 复核）：①内核 grep 454/1484/1487 无硬编码
// ②effect_meta 无行（1454 是连击、另一回事）③moves_lib/custom overrides 无注册
// ——真缺口（registry moves 1004 无三者）。
// 先例：2000/610/954/1848/881（选择期条件先制祖模板）、2298/779/1860（路由表 +
// 无提升判据，J11/J29）、crawford 1397（低体 hp×denom<max 口径）、J26 shield_bank_。
// 场景回归：test/scenario/scenario_1390_cond_priority.cpp（1390 号段，一文件三宏）。

#include "common.h"

namespace {

// ── 454 —— 当自身血量少于1/{0}时先制+{1}（选择期路由）──
EffectResult effect_454_lowhp_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 454, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 454, args,
                     "布局=[owner,target,除数,先制量]");
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 454, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(denom > 0 && bonus > 0, 454, args, "需要 除数/先制量 > 0");
    // 严格低于 1/{0}：hp×{0} < max_hp（1397 同款整数直乘，无除法截断）。
    if (ctx->getPet(self).hp * denom >= max_hp_of(ctx, self)) {
        return EffectResult::kOk;
    }
    ctx->ws.preemptive_level[self] += bonus;
    return EffectResult::kOk;
}

// ── 1484 —— 体力低于最大体力的1/2时先制+1（零参，454 定档 denom=2 bonus=1）──
EffectResult effect_1484_halfhp_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1484, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1484, args,
                     "布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1484, args, "需要 owner ∈ {0,1}");
    if (ctx->getPet(self).hp * 2 >= max_hp_of(ctx, self)) {
        return EffectResult::kOk;   // 体力 ≥ 1/2 → 不触发
    }
    ctx->ws.preemptive_level[self] += 1;
    return EffectResult::kOk;
}

// ── 1487 —— 自身处于护盾状态下先制+1（零参）──
EffectResult effect_1487_shield_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1487, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1487, args,
                     "布局=[owner,target]（on_selected 绑定）");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1487, args, "需要 owner ∈ {0,1}");
    if (ctx->getPet(self).shield_bank_.total() <= 0) {
        return EffectResult::kOk;   // 无护盾 → 不触发
    }
    ctx->ws.preemptive_level[self] += 1;
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(454, effect_454_lowhp_priority)
SKILL_EFFECT(1484, effect_1484_halfhp_priority)
SKILL_EFFECT(1487, effect_1487_shield_priority)
