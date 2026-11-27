#include <utils/sbs_rand.h>
// 回血 / 固伤 / 吸血 / 伤害修正（保底 447 / 递增 906 / 低伤条件 1256 / 吸取减伤 1287）。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

namespace {

// Effect 497 - 附加{0}点固定伤害，每次使用额外附加{1}点，最高{2}点，遇到天敌时效果翻倍（落芳天华 17912）。
// Args: [owner(0), target(1), base(2), step(3), cap(4)]（记录 args=[100,100,400] + bind_participants 前缀）。
// 计数放 `pet.on_stage_storage`（**本次上场**槽，换宠由引擎清空）——官方实测：这类"每次使用递增"
// **下场不保留**（用户 2026-09-13 确认）；需要跨切换累计的（如无相谛 2260 蓄力）才用 soulmark_storage。
EffectResult effect_497_increasing_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    // 本次上场使用次数（含本次）：第 1 次 = base，之后每次 +step，封顶 cap。
    auto& storage = ctx->getPet(owner).on_stage_storage;
    constexpr int kCounterKey = 497;
    if (!storage.count(kCounterKey)) {
        storage[kCounterKey] = 0;
    }
    int& uses = std::any_cast<int&>(storage[kCounterKey]);
    ++uses;
    int amount = base + step * (uses - 1);
    if (cap > 0 && amount > cap) {
        amount = cap;   // "最高{2}点"——先封顶
    }
    if (is_predator(ctx, owner)) {
        amount *= 2;    // 遇到天敌时效果翻倍（封顶之后再翻倍）
    }
    if (amount > 0 && g_core && g_core->fixed_damage) {
        g_core->fixed_damage(ctx, target, amount);
    }
    return EffectResult::kOk;
}

// Effect 1287 - 吸取对手能力提升，吸取成功则下{0}次受到的攻击伤害减少{1}%（烬灭神咒剑）。
// Args: [owner(0), target(1), times(2), pct(3)]（record.args=[N,P] + bind_participants 前缀）。
// 走 transfer_stat_boosts 真原语（吸取 = 转化：对手正等级搬到自己身上，对手清零）——
// 一并拿到"本体/视图同步"与"免消除强化(STAT_CLEAR)查询"（对手有该保护时吸取失败）。
EffectResult effect_1287_drain_and_reduction(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int times = args.int_args[2];
    const int pct = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || times <= 0) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/target, /*to=*/owner) <= 0) {
        return EffectResult::kOk;  // 无可吸取 或 对手受免消除强化保护
    }
    // 吸取成功则下 times 次受击减伤 pct%（**百分比减伤**阶段 REDUCE_PCT，count 递减）。
    // 只在 owner 作为受击方(defenderId==owner)时减伤——否则 owner 自己攻击时也触发，
    // 白耗次数。仿 effect_2006 的 defenderId 判定。
    auto remaining = std::make_shared<int>(times);
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [remaining, pct, owner](BattleContext* c, int) {
            if (*remaining <= 0) {
                return;
            }
            if (c->resolvedDamage.defenderId != owner) {
                return;  // 只减 owner 受击的伤害
            }
            c->resolvedDamage.final = c->resolvedDamage.final * (100 - pct) / 100;
            --*remaining;
        });
    return EffectResult::kOk;
}

// effect 683 - 随机附加 {low}~{high} 点固定伤害（诸邪皆伏）。Args=[owner,target,low,high]。
EffectResult effect_683_random_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int low = args.int_args[2];
    const int high = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || low < 0 || high < low) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->fixed_damage) {
        const int amt = choose_skill_range(*ctx, 683, 0, owner, target, low, high);  // [low, high] 闭区间
        g_core->fixed_damage(ctx, target, amt);
    }
    return EffectResult::kOk;
}

// effect 1256 - 造成的伤害低于 {threshold} 时令对手 {anomaly}，未触发则自身下 {count} 次攻击伤害提升 {pct}%。
// 挂 AFTER_ACTION（伤害结算后），读 resolvedDamage.final = 本回合该攻击伤害。王·酷烈风息。
EffectResult effect_1256_low_damage_boost(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int threshold = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int count = args.int_args[4];
    const int pct = args.int_args[5];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const int dmg = ctx->resolvedDamage.final;  // 本回合该攻击的最终伤害
    if (dmg < threshold) {
        if (g_core && g_core->apply_anomaly) {
            g_core->apply_anomaly(ctx, target, anomaly, /*duration=*/2, owner);
        }
    } else {
        ctx->grant_attack_boost(owner, /*source_effect_id=*/1256, /*count=*/count, /*pct=*/pct);
    }
    return EffectResult::kOk;
}

// ── Effect 906 - 附加对手最大体力值{0}%的百分比伤害，每次使用增加{1}%，最高{2}%（神灵之触）──
// Args: [self, target, base, step, cap]
// 计数放 pet 的 on_stage_storage（"本次上场"槽）：下场不保留——与落芳天华 497 同族
// （大多数"每次使用递增"类官方实测下场重置，见 CLAUDE.md §3.10）。
EffectResult effect_906_growing_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int base = args.int_args[2];
    const int step = args.int_args[3];
    const int cap = args.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || base <= 0) {
        return EffectResult::kOk;
    }
    auto& slot = ctx->getPet(self).on_stage_storage[906];
    if (!slot.has_value()) {
        slot = 0;   // 本场已用次数
    }
    const int used = any_int_or(slot);
    int pct = base + used * step;
    if (cap > 0 && pct > cap) {
        pct = cap;   // "最高{2}%"
    }
    slot = used + 1;
    if (g_core && g_core->deal_pink_damage) {
        // PERCENT：amount 是**最大体力的百分比**（引擎按目标最大体力换算）。
        g_core->deal_pink_damage(ctx, opp, pct, DamageKind::PERCENT, self);
    }
    return EffectResult::kOk;
}

// ── Effect 1005 - 恢复自身最大体力的1/{0}并造成等量百分比伤害（神圣复苏）──
// Args: [self, target, denom]
// "等量" = 与**实际**恢复量相等（吃封回血/恢复修正后的真实值），故用恢复前后的 HP 差。
// PERCENT_VALUE：amount 已是具体值（不是最大体力百分比），见 DamageKind。
EffectResult effect_1005_heal_and_equal_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int healed = heal_max_hp_fraction(ctx, self, denom);
    if (healed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

// ── Effect 1239 - 恢复自身最大体力的1/{0}，自身体力低于1/{1}时造成等量百分比伤害（光荣之梦）──
// Args: [self, target, denom_heal, denom_threshold]
// 阈值判定用**恢复前**的体力（恢复会把体力抬上去，用恢复后判会自我否定）。
EffectResult effect_1239_heal_or_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom_heal = args.int_args[2];
    const int denom_threshold = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom_heal <= 0 || denom_threshold <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const bool below = ctx->getPet(self).hp * denom_threshold < max_hp;   // 体力低于 1/N
    const int healed = heal_max_hp_fraction(ctx, self, denom_heal);
    if (below && healed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

// ── effect 29 「额外附加{0}点固定伤害」—— 元素 11261（200）/ 断空破 10994（30）──
// Args = [owner, target, 数值]。走粉伤原语（吃护罩/固定抗性/固定减粉，即时结算）。
EffectResult effect_29_fixed_extra(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── effect 43 「恢复自身最大体力的1/{0}」—— 轮回 20607（Args=[1] = 全部）──
// Args = [owner, target, 分母]。走 heal 原语（吃封回血 + 恢复修正 + 记 last_heal）。
EffectResult effect_43_heal_max_ratio(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, owner);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, max_hp / denom);
    }
    return EffectResult::kOk;
}

// ── effect 28 「降低对方1/{0}的hp」—— 圣灵魔闪光 11000（Args=[8]）──
// Args = [owner, target, 分母]。★ 走 `PERCENT_VALUE`（**值已算好**的百分比档）而不是
// `PERCENT`：`PERCENT` 的 amount 是整数百分比，1/8 只能表达成 12%（少算），
// 而 `PERCENT_VALUE` 传 `max_hp / 8` 是精确的 1/8，且**仍走百分比抗性/护罩**。
EffectResult effect_28_lower_opp_hp_ratio(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int denom = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, target);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, max_hp / denom, DamageKind::PERCENT_VALUE, owner);
    }
    return EffectResult::kOk;
}

// effect 447 —— 造成的伤害不少于{0}（131 个技能在用，"保底伤害"）。
// SKILL_EFFECT 时点把保底值**赋值**进 ws.damage_floor[owner]（每回合 memset 覆盖；
// 管线在本时点之后跑），伤害管线 FLOOR 阶段默认条目（install_default_damage_floor）
// 读它把减伤后的红伤抬回 {0}。打盔/miss 不跑管线 → 天然不触发。
EffectResult effect_447_damage_floor(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int floor_value = args.int_args[2];
    if (owner < 0 || owner > 1 || floor_value <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_floor[owner] = floor_value;
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 批 3：概率固伤 / 吸血 / 先后手威力 / 连用递增（2026-09-17，≈630 个技能引用）
//   93  {0}%概率额外附加{1}点固伤（168 技）
//   101 伤害数值的{0}%恢复自身体力（126 技）
//   40  先出手的话威力为2倍（137 技）/ 30 后出手的话威力为2倍（93 技）
//   9   连续使用每次威力增加{0}，最高威力{1}（103 技）
// ════════════════════════════════════════════════════════════════════

// ── effect 93 —— {0}%概率额外附加{1}点固定伤害（迷之气泡 12325 等）──
// Args=[owner,target,chance,amount]。SKILL_EFFECT 时点直通（同 29/683 附加固伤先例）；
// 概率掷点按"短路径不消耗 rng.next()"纪律走 roll_percent（100% 短路）。
EffectResult effect_93_chance_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (roll_skill_percent(*ctx, 93, 0, owner, target, chance) && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ── effect 101 —— 给对手造成伤害时，伤害数值的{0}%恢复自身体力（魔王炎击波 10377 等）──
// Args=[owner,target,pct]。挂 AFTER_ACTION（1256 先例：伤害结算后读 resolvedDamage.final，
// 连击 ×N 已含在 final 里）。回血走 heal_amount（吃封回血/恢复修正）。
// 注册时点在 core 侧 effect_register_state(101) = AFTER_ACTION。
EffectResult effect_101_damage_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int dmg = ctx->resolvedDamage.final;   // 本次攻击的最终红伤
    if (dmg > 0) {
        g_core->heal_amount(ctx, self, dmg * pct / 100);
    }
    return EffectResult::kOk;
}

// ── effect 422 —— 附加**所造成伤害值**{0}%的固定伤害（晦能入侵 38480 等）──
// Args=[owner,target,pct]。挂 AFTER_ACTION（core 侧 effect_register_state(422)）。
//
// ★ 取值 = `ws.raw_attack_damage[owner]`（**裸伤台账**），**不是** `resolvedDamage.final`：
//   前者是公式值（+变威力重算后的第二次结果），不含伤害管线任何阶段；后者是管线后的值，
//   会被锁伤/挡伤/减伤改小。用户 2026-09-18 实测口径："挡伤、锁伤所改变的红伤都不是被
//   这个效果记录的"——所以这一族必须读台账。详见 battleWorkspace.h 的"三个伤害值"说明。
// 附加的那一段走**粉伤**通道（FIXED），吃粉伤抗性/护罩/免粉。
EffectResult effect_422_percent_of_damage_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int raw = ctx->ws.raw_attack_damage[owner];   // 裸伤（本次攻击；无攻击时 0）
    if (raw <= 0) {
        return EffectResult::kOk;
    }
    const int amount = raw * pct / 100;
    if (amount > 0) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 批 4：随机回血 / 全属性提升 / 全属性弱化（2026-09-17，≈1120 个技能引用）
//   450 随机恢复{0}到{1}点生命值（309 技）
//   585 技能使用成功时，{0}——六项属性各带等级的提升模板（681 技）
//   749 {0}%令对手全属性-{1}（133 技）
// ════════════════════════════════════════════════════════════════════

// ── effect 450 —— 随机恢复{0}到{1}点生命值（幸运百宝袋 22129 等）──
// Args=[owner,target,lo,hi]。区间均匀掷点（掷点在**动作分支内**消耗 rand——本效果
// 无概率门，每次必掷）；走 heal_amount（吃封回血/恢复修正，内部钳满）。
EffectResult effect_450_random_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int lo = args.int_args[2];
    const int hi = args.int_args[3];
    if (self < 0 || self > 1 || lo <= 0 || hi < lo) {
        return EffectResult::kOk;
    }
    const int amount = choose_skill_range(*ctx, 450, 0, self, self, lo, hi);
    g_core->heal_amount(ctx, self, amount);
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    SKILL_EFFECT(1287, effect_1287_drain_and_reduction);       // 吸取减伤
    SKILL_EFFECT(497, effect_497_increasing_fixed);           // 固伤递增+天敌翻倍(落芳天华)
    SKILL_EFFECT(683, effect_683_random_fixed);              // 随机附加N~M点固定伤害(诸邪皆伏)
    SKILL_EFFECT(1256, effect_1256_low_damage_boost);        // 低伤→焚烬/未触发→次数攻击增伤(王·酷烈风息)
    SKILL_EFFECT(906, effect_906_growing_percent);            // 神灵之触: 递增百分比伤害
    SKILL_EFFECT(1005, effect_1005_heal_and_equal_percent);   // 神圣复苏: 回血+等量百分比
    SKILL_EFFECT(1239, effect_1239_heal_or_percent);          // 光荣之梦: 回血/低于阈值附加百分比
    SKILL_EFFECT(28, effect_28_lower_opp_hp_ratio);     // 圣灵魔闪光: 降低对方1/8 hp
    SKILL_EFFECT(29, effect_29_fixed_extra);            // 元素/断空破: 额外固定伤害
    SKILL_EFFECT(43, effect_43_heal_max_ratio);         // 轮回: 恢复最大体力1/N
    SKILL_EFFECT(447, effect_447_damage_floor);        // 造成的伤害不少于{0}(保底伤害)

    // ── 批 3：概率固伤 / 吸血 / 先后手威力 / 连用递增（2026-09-17）──
    SKILL_EFFECT(93, effect_93_chance_fixed);          // {0}%概率额外附加{1}点固定伤害
    SKILL_EFFECT(101, effect_101_damage_heal);         // 伤害的{0}%回血(AFTER_ACTION)
    SKILL_EFFECT(422, effect_422_percent_of_damage_fixed);   // 附加所造成伤害值{0}%的固定伤害（读裸伤台账）

    // ── 批 4：随机回血 / 全属性提升 / 全属性弱化（2026-09-17）──
    SKILL_EFFECT(450, effect_450_random_heal);         // 随机恢复{0}到{1}点生命值

}  // namespace
