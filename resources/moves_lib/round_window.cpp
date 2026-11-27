// 回合窗口与循环族："N回合内每回合…"(57/77/60/532/597/707/677)、"下N回合…"
// (694/776/843)、窗口免伤免暴(525/58/86)、先制基值(843)。
//
// （2026-09-17 从 lib_1.cpp 按家族拆分；移动零改动，行为由全量场景回归守护。）
#include "common.h"

#include <any>
#include <utility>

namespace {

EffectResult effect_skill_843(BattleContext* context, const EffectArgs& args) {
    if (!context || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }

    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || bonus == 0) {
        return EffectResult::kOk;
    }

    // 注册到技能桶（source_id=843 → 同源去重）。
    // valid_id_ 绑定、回合计数、key 铸造、去重覆盖都由 register_skill_effect 内部完成
    // （引擎侧 inline 入口，插件不链接 sim_core 也能调）。
    Effect effect(
        843, 0, self, rounds,
        EffectArgs(std::vector<int>{self, 1 - self, bonus}),
        &effect_move_right_priority_bonus
    );
    // 窗口起点按官方"回合数判定"折算：843 原文是「**下**{0}回合」（next_rounds 家族）→
    // 本回合不算、先/后出手都从下一回合起算。原先写死 context->roundCount 会**少一回合**
    // （"下2回合"实际只覆盖到 1 个 MOVE_RIGHT）——窗口起点 = R+1 后才是 R+1..R+N。
    const EffectWindowKind window_kind = (g_core && g_core->effect_window_kind)
        ? g_core->effect_window_kind(843)
        : EffectWindowKind::InRounds;
    const int start_round = context->round_effect_start_round(self, rounds, window_kind);
    auto ce = std::make_unique<ContinuousEffect>(
        effect, State::BATTLE_FIRST_MOVE_RIGHT, self, rounds, start_round
    );
    ce->source_id_ = 843;
    context->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, self, std::move(ce));

    return EffectResult::kOk;
}

// ── Effect 525 - {0}回合内有{1}%概率免疫对手的攻击伤害（璨灵圣光）──
// Args: [self, target, rounds, pct]
// ⚠️ 走 BLOCK 阶段管线而不是 RuleCenter 的窗口型 DAMAGE 免疫：两者语义差别在
//   ①概率（免疫内核只有"必定"，这里是 {1}% 逐次掷）；②挡伤失效（BLOCK 类别吃
//   damage_suppress_mask，蚀砚之泪≥4滴照样压制它）。窗口用 roundCount 半开区间判。
// ⚠️ 已知边界：管线回调注册后**不随断回合消失**（它不在时点桶里），窗口到期后自身失效但
//   条目仍驻留到 clearAllEffects。断回合是否该解除此类技能免伤，待与官方口径核对后再定。
EffectResult effect_525_attack_damage_immune(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::BLOCK,
        self,
        DamageEffectCategory::BLOCK,
        [self, start, expire, pct](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            if (c->roundCount < start || c->roundCount >= expire) {
                return;   // 窗口外（含已过期）——条目仍在，但不再产生作用
            }
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                return;   // 只免疫"对手的攻击伤害"（红伤）
            }
            // 归属护盾持有方，目标取本次来袭攻击的实际发起方。
            if (!roll_skill_percent(*c, 525, 0, self, damage.attackerId, pct)) {
                return;   // {1}% 未中
            }
            damage.base = damage.afterAdd = damage.afterMul = damage.final = 0;
            damage.addPct = 0;
            damage.mulCoef = 0.0;
        });
    return EffectResult::kOk;
}

// ── Effect 57 - {0}回合内每回合使用技能恢复自身最大体力的1/{1}（璨灵圣光）──
// Args: [self, target, rounds, denom]
// 挂在 FIRST/SECOND 两个"行动后"时点：桶按 (时点, 行动方) 取，注册两处 →
// 无论本回合先手还是后手，恰好触发一次。
EffectResult effect_57_sustain_heal(BattleContext* ctx, const EffectArgs& args);

EffectResult effect_57_sustain_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;   // "每回合**使用技能**恢复"——没出招不恢复
    }
    heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

EffectResult effect_57_sustain_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 57, rounds, {self, 1 - self, denom},
                              &effect_57_sustain_heal_tick);
    }
    return EffectResult::kOk;
}

// ── Effect 694 - 下{0}回合攻击必定先出手（璨灵圣光）──
// Args: [self, target, rounds]
// "必定先出手" 比先制等级更强 → 写 ws.guaranteed_first（先手权比较时优先于 preemptive_level）。
EffectResult effect_694_guaranteed_first(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 694, rounds,
                          {self, 1 - self, /*tier=*/9}, &effect_set_guaranteed_first);
    return EffectResult::kOk;
}

// ── Effect 776 - 下{0}回合自身造成的攻击伤害翻倍（璨灵圣光）──
// Args: [self, target, rounds]
// 每回合在 ROUND_START 往 ws.damage_add_pct 累加 100（= ×2），AMP 阶段管线读取。
// 放 ws 而非管线回调：ws 每回合 reset 天然清空，效果走时点桶 → 断回合/切换作废免费获得。
EffectResult effect_776_damage_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 776, rounds,
                          {self, 1 - self, /*pct=*/100}, &effect_set_damage_amp);
    return EffectResult::kOk;
}

// ── Effect 693 - 下{0}回合造成的攻击伤害**额外**提升{1}%（圣光吟诵）──
// Args: [self, target, rounds, pct]
// ⚠️ 原文带"**额外**" → 官方 L352 的**非通用增伤 = 乘法**通道（`damage_add_extra_mul`），
//    落在 AMP_EXTRA 阶段——**排在通用增伤之后**。这正是"693 增伤乱穿犀牛"（L348）的结构：
//    犀牛的受高伤检测在链首（GUARD_DETECT），之后才被 693 抬起来的伤害它看不到。
//    （对照 776「攻击伤害翻倍」不带"额外" → 通用增伤 = 加法通道/AMP 阶段，写法不变。）
EffectResult effect_693_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || pct == 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 693, rounds,
                          {self, 1 - self, pct}, &effect_set_damage_amp_extra);
    return EffectResult::kOk;
}

// ── Effect 677 - {0}回合内每回合使用技能吸取对手能力提升状态（光荣之梦）──
// Args: [self, target, rounds]
EffectResult effect_677_drain_boosts_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {   // 注册时只带 {self, opp}
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->transfer_stat_boosts) {
        g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/self);
    }
    return EffectResult::kOk;
}

EffectResult effect_677_drain_boosts(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 677, rounds, {self, opp}, &effect_677_drain_boosts_tick);
    }
    return EffectResult::kOk;
}

// ── effect 60 「{0}回合内每回合都能附加{1}点固定伤害」—— 旋灭裂空阵 10995（Args=[5,30]）──
// 与 1531 同套路：注册到"每回合使用技能"的两个时点格，同回合只有一格以 owner 为 mover。
constexpr int kEffect60Ticks = 60;

EffectResult effect_60_tick(BattleContext* ctx, const EffectArgs& args) {
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

EffectResult effect_60_round_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || rounds <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, kEffect60Ticks,
                          rounds, {owner, target, amount}, &effect_60_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, kEffect60Ticks,
                          rounds, {owner, target, amount}, &effect_60_tick);
    return EffectResult::kOk;
}

// ── effect 77 「{0}回合内若使用技能则恢复{1}点体力值」—— 生命 20606（Args=[5,100]）──
constexpr int kEffect77Ticks = 77;

EffectResult effect_77_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->heal_amount) {
        g_core->heal_amount(ctx, owner, amount);
    }
    return EffectResult::kOk;
}

EffectResult effect_77_round_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || rounds <= 0 || amount <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_FIRST_SKILL_EFFECT, owner, kEffect77Ticks,
                          rounds, {owner, 1 - owner, amount}, &effect_77_tick);
    register_round_effect(ctx, State::BATTLE_SECOND_SKILL_EFFECT, owner, kEffect77Ticks,
                          rounds, {owner, 1 - owner, amount}, &effect_77_tick);
    return EffectResult::kOk;
}

// ── effect 58 「下{0}回合自身攻击技能必定打出致命一击」—— 圣光气 20500（Args=[2]）──
// Args = [owner, target, 回合数]。
// ★ 走 `ws.must_crit[owner]`（**每回合 reset** → 效果用"回合类"每回合重写，见 §3.8 的
//   "下N回合"套路）。为什么不用 `ws.crit_rate_mod`：那是**乘算**修正，技能自身
//   `crit_rate == 0` 时 0 × 任何数 = 0，表达不了"必定"（`crit_rate==0` 是"永不必暴"）。
constexpr int kEffect58Ticks = 58;

EffectResult effect_58_tick_write(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[owner] = true;
    return EffectResult::kOk;
}

EffectResult effect_58_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // 写在**回合开始**（早于本回合的技能使用，且 ws 刚 reset 完）→ 本回合立刻生效。
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, kEffect58Ticks,
                          rounds, {owner, 1 - owner}, &effect_58_tick_write);
    return EffectResult::kOk;
}

// ── effect 86 「{0}回合内属性攻击对自身必定miss」—— 圣洁 20605（Args=[5]）──
// Args = [owner, target, 回合数]。
// ★ 写 `ws.attribute_must_miss[owner]`：owner = **被保护方**，对手出手时查 `1 - attacker`。
//   ⚠️ 这是"必定 **miss**"（技能打空），不是"失效"——两者不同：miss 会消费对方的
//     次数类盔（文档 §2.3），而"失效"走 SKILL_INVALID 补偿分支。
constexpr int kEffect86Ticks = 86;

EffectResult effect_86_tick_write(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.attribute_must_miss[owner] = true;
    return EffectResult::kOk;
}

EffectResult effect_86_attribute_miss(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int rounds = args.int_args[2];
    if (owner < 0 || owner > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_round_effect(ctx, State::BATTLE_ROUND_START, owner, kEffect86Ticks,
                          rounds, {owner, 1 - owner}, &effect_86_tick_write);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 批 2：回合窗口循环族 + 全属性族（2026-09-17，≈1210 个技能引用）
//   532 每回合用技能回血+等量百分比伤害（236 技）
//   597 每回合用技能吸取对手最大体力 1/N（201 技）—— CoreApi 新槽 drain_hp
//   707 每回合用技能回血，低血翻倍（169 技）
//   885/946/962/1001/1014 全属性+N，条件则翻倍（604 技）
// 窗口循环节奏同 57/677 先例：FIRST/SECOND_AFTER_ACTION 各注册一 ticks、
// `used_skill_this_round` 门（"每回合**使用技能**"——没出招不结算）。
// ════════════════════════════════════════════════════════════════════

// ── effect 532 —— {0}回合内每回合使用技能恢复自身最大体力的1/{1}并造成等量百分比伤害 ──
// Args=[owner,target,rounds,denom]。等量 = **实际回到的血**（满血/被封回血 → 回 0 → 无伤害，
// "并造成等量"语义绑定）。百分比伤害走 PERCENT_VALUE（具体值、百分比档抗性/护罩）。
EffectResult effect_532_heal_equal_percent_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const int healed = heal_max_hp_fraction(ctx, self, denom);
    if (healed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

EffectResult effect_532_heal_equal_percent(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
            return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
            register_round_effect(ctx, st, self, 532, rounds, {self, opp, denom},
                              &effect_532_heal_equal_percent_tick);
    }
    return EffectResult::kOk;
}

// ── effect 597 —— {0}回合内每回合使用技能吸取对手最大体力的1/{1} ──
// Args=[owner,target,rounds,denom]。drain_hp 原语（CoreApi 槽）：对手掉固定伤、自身回等量。
EffectResult effect_597_round_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (g_core && g_core->drain_hp) {
        g_core->drain_hp(ctx, /*actor=*/self, /*target=*/opp, denom);
    }
    return EffectResult::kOk;
}

EffectResult effect_597_round_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 597, rounds, {self, opp, denom},
                              &effect_597_round_drain_tick);
    }
    return EffectResult::kOk;
}

// ── effect 707 —— {0}回合内每回合使用技能恢复自身最大体力的1/{1}，
//                   恢复体力时若自身体力低于最大体力的1/{2}则恢复效果翻倍 ──
// Args=[owner,target,rounds,denom,low_denom]。"恢复效果翻倍" = 恢复量 ×2（heal_amount 内部钳满）。
EffectResult effect_707_round_heal_double_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const int base = max_hp / denom;
    if (base <= 0 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const bool low = ctx->getPet(self).hp < max_hp / std::max(1, low_denom);
    g_core->heal_amount(ctx, self, low ? base * 2 : base);
    return EffectResult::kOk;
}

EffectResult effect_707_round_heal_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    if (self < 0 || self > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 707, rounds, {self, 1 - self, denom, low_denom},
                              &effect_707_round_heal_double_tick);
    }
    return EffectResult::kOk;
}


// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    SKILL_EFFECT(843, effect_skill_843);   // 下N回合自身所有技能先制+N

    // ── 批 3：圣灵谱尼专属技能 ──────────────────────────────────────
    SKILL_EFFECT(525, effect_525_attack_damage_immune);        // 璨灵圣光: N回合内概率免攻击伤害
    SKILL_EFFECT(57, effect_57_sustain_heal);                 // 璨灵圣光: N回合内每回合用技能回血
    SKILL_EFFECT(694, effect_694_guaranteed_first);           // 璨灵圣光: 下N回合必定先出手
    SKILL_EFFECT(776, effect_776_damage_double);              // 璨灵圣光: 下N回合攻击伤害翻倍
    SKILL_EFFECT(693, effect_693_damage_up);                  // 圣光吟诵: 下N回合攻击伤害+{X}%
    SKILL_EFFECT(677, effect_677_drain_boosts);               // 光荣之梦: N回合内每回合吸取强化
    SKILL_EFFECT(58, effect_58_must_crit);              // 圣光气: 下N回合必定暴击
    SKILL_EFFECT(60, effect_60_round_fixed);            // 旋灭裂空阵: N回合每回合附加
    SKILL_EFFECT(77, effect_77_round_heal);             // 生命: N回合用技能则回血
    SKILL_EFFECT(86, effect_86_attribute_miss);         // 圣洁: N回合属性攻击必miss

    // ── 批 2：回合窗口循环族 + 全属性族（2026-09-17）──
    SKILL_EFFECT(532, effect_532_heal_equal_percent);  // N回合内每回合用技能回血+等量百分比伤害
    SKILL_EFFECT(597, effect_597_round_drain);         // N回合内每回合用技能吸取对手最大体力1/N
    SKILL_EFFECT(707, effect_707_round_heal_double);   // N回合内每回合用技能回血,低血翻倍

    // ── 批 5：状态窗口族（2026-09-17）──
    // （实现体在本文件尾部；此处前向声明供注册宏引用）
    EffectResult effect_32_crit_rate_up(BattleContext* ctx, const EffectArgs& args);
    EffectResult effect_408_damage_reduce(BattleContext* ctx, const EffectArgs& args);
    EffectResult effect_426_dodge(BattleContext* ctx, const EffectArgs& args);
    // ── 批 7：下N回合增益 + 对手限制窗口族（2026-09-17）──
    EffectResult effect_481_next_rounds_priority(BattleContext* ctx, const EffectArgs& args);
    EffectResult effect_681_next_rounds_must_crit_hit(BattleContext* ctx, const EffectArgs& args);
    EffectResult effect_478_seal_opp_attribute(BattleContext* ctx, const EffectArgs& args);
    EffectResult effect_679_opp_heal_block(BattleContext* ctx, const EffectArgs& args);
    EffectResult effect_125_damage_cap(BattleContext* ctx, const EffectArgs& args);
    SKILL_EFFECT(481, effect_481_next_rounds_priority);  // 下N回合自身攻击先制+X
    SKILL_EFFECT(681, effect_681_next_rounds_must_crit_hit); // 下N回合攻击必定致命、必中
    SKILL_EFFECT(478, effect_478_seal_opp_attribute);    // N回合内对手属性技能无效
    SKILL_EFFECT(679, effect_679_opp_heal_block);        // N回合内对手无法通过技能回血
    SKILL_EFFECT(125, effect_125_damage_cap);            // N回合内被攻击伤害不超过M
    SKILL_EFFECT(32, effect_32_crit_rate_up);          // N回合攻击要害概率+1/16(暴击加算)
    SKILL_EFFECT(408, effect_408_damage_reduce);       // N回合内每回合所受伤害减少{1}点
    SKILL_EFFECT(426, effect_426_dodge);               // N回合内每回合{1}%闪避对手攻击

// ════════════════════════════════════════════════════════════════════
// 批 5：状态窗口族（2026-09-17，≈330 个技能引用）
//   32  使用后{0}回合攻击要害概率+1/16（暴击**加算**通道）
//   408 {0}回合内每回合所受伤害减少{1}（点数减伤，管线 REDUCE_FLAT + 窗口判定）
//   426 {0}回合内每回合{1}%闪避对手攻击（ws.dodge_rate）
// in_rounds 首回合生效套路：效果体**立即写**当回合 ws（先手当回合即受益），
// 再注册 ROUND_START 窗口 tick 供后续回合重写（ws 每回合 memset）；后手顺延由
// round_effect_start_round 折算（官方 L313"后出手回合数+1"）。
// ════════════════════════════════════════════════════════════════════

// ── effect 32 —— 使用后{0}回合攻击击中对象要害概率增加1/16（蓄气 20039 等，97 技）──
// Args=[owner,target,rounds]。+1/16 = +6.25 个百分点走 ws.crit_rate_add（加算通道）。
EffectResult effect_32_crit_rate_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    ctx->ws.crit_rate_add[args.int_args[0]] = 6.25f;
    return EffectResult::kOk;
}
EffectResult effect_32_crit_rate_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.crit_rate_add[self] = 6.25f;   // 当回合立即生效
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 32, rounds,
                          {self, 1 - self}, &effect_32_crit_rate_tick);
    return EffectResult::kOk;
}

// ── effect 408 —— {0}回合内每回合所受的伤害减少{1}（大气护盾 21720 等，97 技）──
// Args=[owner,target,rounds,reduce]。无 "%" → **点数**减伤。管线 REDUCE_FLAT 条目 +
// roundCount 半开区间窗口判（MITIGATE 不可被"挡伤失效"无视）。
// ⚠️ **重复使用 = 刷新窗口**（官方语义），不叠加：窗口起止按值存入本局 plugin_storage，
// 管线条目仍每方只注册一次且沿用首次减免值；回调按 key 从当次 ctx 读取窗口。
EffectResult effect_408_damage_reduce(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int reduce = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || reduce <= 0) {
        return EffectResult::kOk;
    }
    using Win = std::pair<int, int>;   // [start, expire)
    const int key = 408000 + self;
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const Win window{start, start + rounds};
    const auto [it, inserted] = ctx->plugin_storage.try_emplace(key, window);
    if (!inserted) {
        // 包括窗口到期或换宠后：仍只刷新窗口，不重新注册。
        std::any_cast<Win&>(it->second) = window;
        return EffectResult::kOk;
    }
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT,
        self,
        DamageEffectCategory::MITIGATE,
        [self, reduce, key](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            const auto entry = c->plugin_storage.find(key);
            if (entry == c->plugin_storage.end()) {
                return;
            }
            const auto* win = std::any_cast<Win>(&entry->second);
            const int rc = c->roundCount;
            if (!win || rc < win->first || rc >= win->second) {
                return;   // 无有效窗口或窗口外
            }
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                return;   // 只减"所受的攻击伤害"（红伤）
            }
            damage.final = std::max(0, damage.final - reduce);
        }, EffectScope::ON_STAGE, BattleCallbackCopy::Audited);
    return EffectResult::kOk;
}

// ── effect 426 —— {0}回合内每回合{1}%闪避对手攻击（暗尘弥散 21878 等，99 技）──
// Args=[owner,target,rounds,pct]。写 ws.dodge_rate（命中公式：hit = accuracy - dodge×100）。
EffectResult effect_426_dodge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    // ⚠️ 注册载荷是 {self, 1-self, pct} → pct 在 **args[2]**（读 args[1] 会拿到对手方编号）。
    ctx->ws.dodge_rate[args.int_args[0]] = args.int_args[2] / 100.0f;
    return EffectResult::kOk;
}
EffectResult effect_426_dodge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || pct <= 0) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = pct / 100.0f;   // 当回合立即生效
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 426, rounds,
                          {self, 1 - self, pct}, &effect_426_dodge_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 批 7：下N回合增益 + 对手限制窗口族（2026-09-17，≈471 技能引用）
//   481(112技) 下{0}回合自身**攻击**先制+{1}          —— MOVE_RIGHT 窗口 + 攻击限定
//   681(88技)  下{0}回合自身**攻击技能**必定致命、必定命中 —— ROUND_START(必暴) + MOVE_RIGHT(必中)
//   478(84技)  {0}回合内令对手使用的**属性技能无效**    —— seal_skill 回合型
//   679(101技) {0}回合内对手**无法通过自身技能恢复体力** —— HEAL_BLOCK 票挂对手
//   125(86技)  {0}回合内每回合被攻击所受到的伤害**不超过{1}** —— 管线 CAP 阶段（697 凭证可穿）
// "下N回合"窗口起点一律用 register_next_rounds_effect（本回合不算）；
// "N回合内"用 register_round_effect（先手含当回合、后手顺延）。
// ════════════════════════════════════════════════════════════════════

// ── effect 481 tick —— 窗口内每回合的 MOVE_RIGHT：攻击技能才加先制 ──
EffectResult effect_481_priority_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int priority = args.int_args[2];
    if (self < 0 || self > 1 || priority == 0) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;   // "自身**攻击**先制+X"：属性技能不吃
    }
    ctx->ws.preemptive_level[self] += priority;
    return EffectResult::kOk;
}
EffectResult effect_481_next_rounds_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int priority = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || priority == 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 481, rounds,
                                {self, 1 - self, priority}, &effect_481_priority_tick);
    return EffectResult::kOk;
}

// ── effect 681 —— 下{0}回合自身攻击技能必定致命、必定命中 ──
// 两条 ws 通道各需一个每回合重写的 tick（ws 每回合 memset）：
//   · 必暴 → ROUND_START 写 ws.must_crit（暴击判定只对攻击技能跑，无需再限定）
//   · 必中 → MOVE_RIGHT 写 ws.must_hit_grant（凭证物化在 query_usage，晚于本时点）；
//           "攻击技能"限定在这里做（属性技能不该白拿必中）
EffectResult effect_681_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.must_crit[self] = true;
    return EffectResult::kOk;
}
EffectResult effect_681_must_hit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    if (!selected_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;
    }
    ctx->ws.must_hit_grant[self] = true;
    return EffectResult::kOk;
}
EffectResult effect_681_next_rounds_must_crit_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 681, rounds,
                                {self, 1 - self}, &effect_681_must_crit_tick);
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 681, rounds,
                                {self, 1 - self}, &effect_681_must_hit_tick);
    return EffectResult::kOk;
}

// ── effect 478 —— {0}回合内令对手使用的属性技能无效（龙吼震天破 24022 等）──
// Args=[owner,target,rounds]。seal_skill 回合型（count=0 + duration_rounds=N）：
// source=施放方（生命周期锚：施放方换宠清、断施放方回合解封），target=被封方。
EffectResult effect_478_seal_opp_attribute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/478,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/0, /*duration_rounds=*/rounds, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── effect 679 —— {0}回合内对手无法通过自身技能恢复体力（王·异甲同心 25555 等）──
// Args=[owner,target,rounds]。封回血票挂**对手**（HEAL_BLOCK，heal_impl 入口查）。
EffectResult effect_679_opp_heal_block(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/679,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/679);
    return EffectResult::kOk;
}

// ── effect 125 —— {0}回合内每回合被攻击所受到的伤害不超过{1}（锁伤·伤害上限）──
// Args=[owner,target,rounds,cap]。管线 **CAP** 阶段（减伤/保底之后、挡伤之前）——
// damage_pipeline.h 明确：CAP 是"最后再拦一次"，**697「无视伤害限制」凭证可穿**，
// 故回调里先查 `ws.attack_credential[attacker].ignore_damage_limit`。
// MITIGATE 类别 → 不可被"挡伤失效"（damage_suppress_mask）无视。
EffectResult effect_125_damage_cap(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int cap = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || cap <= 0) {
        return EffectResult::kOk;
    }
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    const int expire = start + rounds;
    ctx->register_damage_effect(
        DamagePhase::CAP,
        self,
        DamageEffectCategory::MITIGATE,
        [self, start, expire, cap](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            const int rc = c->roundCount;
            if (rc < start || rc >= expire) {
                return;   // 窗口外
            }
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                return;   // 只锁"所受的攻击伤害"（红伤）
            }
            const int attacker = damage.attackerId;
            if (attacker >= 0 && attacker <= 1
                && c->ws.attack_credential[attacker].ignore_damage_limit) {
                return;   // 官方 697「无视伤害限制」凭证穿透
            }
            if (damage.final > cap) {
                damage.final = cap;
            }
        });
    return EffectResult::kOk;
}

}  // namespace
