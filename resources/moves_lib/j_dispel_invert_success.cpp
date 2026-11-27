#include <utils/sbs_rand.h>
// J10 · 解弱/反转成功族 —— 13 宏/116 载体（2026-10-01，worktree ../agents/j10）
//
// 家族语义：**两道门槛 × 各自后缀**——
//   反转门（6 宏）：stat_reversal(自身下降→提升) == REVERSED 才接后缀（733 先例）：
//     955×12   吸取对手{0}点体力           → 点数吸取（fixed_damage + 按实际扣减回血，2274 口径）
//     1043×17  {0}回合受到的伤害转化为体力  → TAKE_DAMAGE watcher InRounds 窗 1:1 heal（1343/1219 口径）
//     1069×8   {0}回合内躲避所有攻击        → ws.dodge_rate=1.0 直写 + ROUND_START 窗重写（426 pct=100 形）
//     1350×9   对手下{0}回合攻击技能无效     → ATTACK_NULLIFY 票（2390 实证口径 rounds+2/register+1）
//     2297×3   自身全属性+{0}              → all_stats_up（stat_change 六槽真原语）
//     2568×1   对手下{0}次属性技能无效       → seal_skill(attribute=true, count)（1115 计次形）
//   解除门（7 宏）：clear_stat_drops(自身) > 0 才接后缀（1115 先例——清弱化不查免疫，
//     返回清掉的槽数，0 = 本无下降 = "解除未成功"）：
//     1153×10  下{0}回合受到的伤害减少{1}点  → REDUCE_FLAT MITIGATE + plugin_storage 窗
//             （408 同机制；"下N回合"= NextRounds 起点，与 408"含当回合"分流的记档）
//     1183×11  {0}回合内造成的伤害提高{1}%   → ws.damage_add_pct 直写 + ROUND_START 窗重写（1111 形）
//     1312×3   下{0}回合先制+{1}            → FIRST_MOVE_RIGHT ContinuousEffect（843 同构，
//             NextRounds 起点折算；载荷接 common.h effect_move_right_priority_bonus）
//     1352×9   自身全属性+{0}              → all_stats_up
//     1392×10  消除对手回合类效果           → break_round_effects（1032/1234 门槛族同原语）
//     1441×16  对手随机{0}个技能PP归零       → 洗牌取前 {0} 槽 pp_zero_slot（1235 兜底同款）
//     1700×7   自身免疫下{0}次受到的异常     → ANOMALY 次免票（2059/2096 兜底同形）
//
// 前置三查（2026-10-01）：①内核 grep 13 id 无行为硬编码 ②effect_meta 无该批 id 声明
//   ③moves_lib 无既有注册——全族真缺口。载体切片抽查全部吻合（955=[300/350]、
//   1153=[回合,点数]、1183=[回合,%]…），全族 116 载体与工单一致。
// 场景回归：test/scenario/scenario_935_dispel_invert.cpp（935-939 号段）。

#include "common.h"

#include <memory>
#include <vector>

namespace {

// ── 点数吸取（J4 2274 口径：固伤通道吃护盾/免粉，按实际扣减回血、回血吃封回血）──
void j10_drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
}

// ── 随机挑 want 个不同技能槽 PP 归零（1235 兜底 batch_b 同款洗牌）──
void j10_zero_random_pp_slots(BattleContext* ctx, int target, int want, int actor) {
    if (!ctx || !g_core || !g_core->pp_zero_slot || want <= 0) {
        return;
    }
    std::vector<int> slots{0, 1, 2, 3, 4};
    const int n = std::min(want, static_cast<int>(slots.size()));
    for (int i = 0; i < n; ++i) {
        const int j = i + (ctx->rng().next() % (static_cast<int>(slots.size()) - i));
        std::swap(slots[static_cast<std::size_t>(i)], slots[static_cast<std::size_t>(j)]);
        (void)g_core->pp_zero_slot(ctx, target, slots[static_cast<std::size_t>(i)], actor);
    }
}

// ═══════════════ 反转门（stat_reversal == REVERSED）═══════════════

// ── 955 —— 反转成功则吸取对手{0}点体力 ──
EffectResult effect_955_invert_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 955, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 955, args,
                     "955 布局=[owner,target,点数]");
    const int self = args.int_args[0];
    const int points = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && points > 0, 955, args, "需要 点数>0");
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 反转失败（本无下降）→ 不吸取
    }
    j10_drain_points(ctx, self, 1 - self, points);
    return EffectResult::kOk;
}

// ── 1043 —— 反转成功则{0}回合受到的伤害转化为自身体力 ──
EffectResult effect_1043_invert_convert(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1043, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1043, args,
                     "1043 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1043, args, "需要 回合>0");
    if (!g_core || !g_core->stat_reversal || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    // InRounds 窗（含当回合，817 同款注册法），TEAM 作用域跨回合（1343 同款选型）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_DAMAGE, self, ctx->roundCount, rounds, /*once=*/false,
        [self](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.target != self || ev.amount <= 0 || !g_core
                || !g_core->heal_amount) {
                return;
            }
            (void)g_core->heal_amount(c, self, ev.amount);   // 1:1 全额转化（1219 口径）
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// 1069 tick：每回合把 dodge_rate 顶满（ws 每回合 reset → 窗口重写）。
EffectResult j10_dodge_all_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->ws.dodge_rate[self] = 1.0f;   // "躲避所有攻击"（命中公式 dodge×100 封顶）
    return EffectResult::kOk;
}

// ── 1069 —— 反转成功则{0}回合内躲避所有攻击 ──
EffectResult effect_1069_invert_dodge_all(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1069, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1069, args,
                     "1069 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1069, args, "需要 回合>0");
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    // 426 形：当回合立即生效 + ROUND_START 窗重写（ws 每回合 memset）。
    ctx->ws.dodge_rate[self] = 1.0f;
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1069, rounds, {self},
                          &j10_dodge_all_tick);
    return EffectResult::kOk;
}

// ── 1350 —— 反转成功则对手下{0}回合攻击技能无效 ──
// 2390 实证口径（场景 890 T16）：rounds+2 / register=下回合 复合计账恰好覆盖"下{0}回合"。
EffectResult effect_1350_invert_attack_nullify(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1350, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1350, args,
                     "1350 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0, 1350, args, "需要 回合>0");
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    (void)ctx->rule_center_.grant_attack_nullify(
        /*bearer=*/self, /*source_effect_id=*/1350, /*suppress=*/1 - self,
        /*rounds=*/rounds + 2, /*register_round=*/ctx->roundCount + 1, EffectScope::ON_STAGE);
    return EffectResult::kOk;
}

// ── 2297 —— 反转成功则自身全属性+{0} ──
EffectResult effect_2297_invert_all_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2297, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2297, args,
                     "2297 布局=[owner,target,提升]");
    const int self = args.int_args[0];
    const int up = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && up > 0, 2297, args, "需要 提升>0");
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, self, up);
    return EffectResult::kOk;
}

// ── 2568 —— 反转成功则令对手下{0}次属性技能无效 ──
EffectResult effect_2568_invert_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2568, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 2568, args,
                     "2568 布局=[owner,target,次数]");
    const int self = args.int_args[0];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && counts > 0, 2568, args, "需要 次数>0");
    if (!g_core || !g_core->stat_reversal || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    (void)g_core->seal_skill(ctx, /*source=*/self, /*target=*/1 - self, /*effect_id=*/2568,
                             /*attribute=*/true, /*attack=*/false, /*count=*/counts,
                             /*duration_rounds=*/0, /*penetrable=*/true,
                             /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                             /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ═══════════════ 解除门（clear_stat_drops > 0）═══════════════

// 1153 的 REDUCE_FLAT 窗回调（窗口起止存 plugin_storage 的 shared_ptr，重复用=刷新，
// 408 同机制；"下{0}回合"= NextRounds 起点）。
EffectResult effect_1153_flat_reduce_gate(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1153, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1153, args,
                     "1153 布局=[owner,target,回合,点数]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && points > 0, 1153, args,
                     "需要 回合>0 且 点数>0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;   // 解除失败（本无下降）→ 无减伤窗
    }
    using Win = std::pair<int, int>;   // [start, expire)
    const int key = 1153000 + self;
    const int start =
        ctx->round_effect_start_round(self, rounds, EffectWindowKind::NextRounds);
    auto win = std::make_shared<Win>(start, start + rounds);
    if (ctx->plugin_storage.count(key)) {
        *std::any_cast<std::shared_ptr<Win>>(ctx->plugin_storage[key]) = *win;
        return EffectResult::kOk;   // 已有条目：只刷新窗口（408 同款）
    }
    ctx->plugin_storage[key] = win;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, points, win](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            const int rc = c->roundCount;
            if (rc < win->first || rc >= win->second) {
                return;   // 窗口外
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != self || !d.isRed || d.final <= 0) {
                return;
            }
            d.final -= points;   // 点数削减（MITIGATE：不可被"挡伤失效"无视）
            if (d.final < 0) {
                d.final = 0;
            }
        });
    return EffectResult::kOk;
}

// 1183 tick：每回合把"造成的伤害+{1}%"重写回 AMP 加算通道（ws 每回合 reset）。
EffectResult j10_amp_up_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[self] += pct;
    return EffectResult::kOk;
}

// ── 1183 —— 解除成功则自身{0}回合内造成的伤害提高{1}% ──
EffectResult effect_1183_dispel_amp_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1183, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1183, args,
                     "1183 布局=[owner,target,回合,%]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && pct > 0, 1183, args,
                     "需要 回合>0 且 %>0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    // "{0}回合内"（InRounds 含当回合）：当回合直写（先手当回合即受益，426 同款）+ 窗重写。
    ctx->ws.damage_add_pct[self] += pct;
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 1183, rounds,
                          {self, 1 - self, pct}, &j10_amp_up_tick);
    return EffectResult::kOk;
}

// ── 1312 —— 解除成功则自身下{0}回合先制+{1}（843 同构，NextRounds 起点折算）──
EffectResult effect_1312_dispel_priority(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1312, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1312, args,
                     "1312 布局=[owner,target,回合,先制]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && rounds > 0 && bonus > 0, 1312, args,
                     "需要 回合>0 且 先制>0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    Effect effect(1312, 0, self, rounds,
                  EffectArgs(std::vector<int>{self, 1 - self, bonus}),
                  &effect_move_right_priority_bonus);
    const int start_round =
        ctx->round_effect_start_round(self, rounds, EffectWindowKind::NextRounds);
    auto ce = std::make_unique<ContinuousEffect>(effect, State::BATTLE_FIRST_MOVE_RIGHT, self,
                                                 rounds, start_round);
    ce->source_id_ = 1312;
    ctx->register_skill_effect(State::BATTLE_FIRST_MOVE_RIGHT, self, std::move(ce));
    return EffectResult::kOk;
}

// ── 1352 —— 解除成功则自身全属性+{0} ──
EffectResult effect_1352_dispel_all_up(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1352, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1352, args,
                     "1352 布局=[owner,target,提升]");
    const int self = args.int_args[0];
    const int up = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && up > 0, 1352, args, "需要 提升>0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    all_stats_up(ctx, self, up);
    return EffectResult::kOk;
}

// ── 1392 —— 解除成功则消除对手回合类效果 ──
EffectResult effect_1392_dispel_break_round(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1392, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 2, 1392, args,
                     "1392 布局=[owner,target]");
    const int self = args.int_args[0];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1392, args, "需要 owner ∈ {0,1}");
    if (!g_core || !g_core->clear_stat_drops || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->break_round_effects(ctx, 1 - self);
    return EffectResult::kOk;
}

// ── 1441 —— 解除成功则使对手随机{0}个技能PP值归零 ──
EffectResult effect_1441_dispel_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1441, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1441, args,
                     "1441 布局=[owner,target,个数]");
    const int self = args.int_args[0];
    const int slots = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && slots > 0, 1441, args, "需要 个数>0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    j10_zero_random_pp_slots(ctx, 1 - self, slots, self);
    return EffectResult::kOk;
}

// ── 1700 —— 解除成功则自身免疫下{0}次受到的异常状态 ──
EffectResult effect_1700_dispel_anomaly_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1700, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 1700, args,
                     "1700 布局=[owner,target,次数]");
    const int self = args.int_args[0];
    const int counts = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && counts > 0, 1700, args, "需要 次数>0");
    if (!g_core || !g_core->clear_stat_drops) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops(ctx, self) <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/1700, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/counts,
                        /*source_effect_id=*/1700);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(955, effect_955_invert_drain)
SKILL_EFFECT(1043, effect_1043_invert_convert)
SKILL_EFFECT(1069, effect_1069_invert_dodge_all)
SKILL_EFFECT(1350, effect_1350_invert_attack_nullify)
SKILL_EFFECT(2297, effect_2297_invert_all_up)
SKILL_EFFECT(2568, effect_2568_invert_seal_attribute)
SKILL_EFFECT(1153, effect_1153_flat_reduce_gate)
SKILL_EFFECT(1183, effect_1183_dispel_amp_up)
SKILL_EFFECT(1312, effect_1312_dispel_priority)
SKILL_EFFECT(1352, effect_1352_dispel_all_up)
SKILL_EFFECT(1392, effect_1392_dispel_break_round)
SKILL_EFFECT(1441, effect_1441_dispel_random_pp_zero)
SKILL_EFFECT(1700, effect_1700_dispel_anomaly_immune)
