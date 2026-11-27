#include <utils/sbs_rand.h>
// 批 8：剩余头部家族清理（2026-09-23，≈2000 个技能引用）——全部走现有内核原语，
// 插件层零内核改动。按家族：
//   威力视图族   37/645(HP阈值倍率) 88(概率倍率) 925(满体力+%)      —— 40/30 同套路
//   回复/固伤    466(定点回血) 467(条件固伤) 1084(固伤异常翻倍)
//   减伤窗口     463(408 同文本族) 54(受攻伤害×1/N，乘算减伤槽)
//   回合循环族   927/1065/1067(532/707 的低血条件变体)
//   PP/属性窃取  583(随机PP归零) 444(全PP一来一去) 425(随机吸取属性)
//   免疫/封属    570(次数免攻击) 980(先手当回合封属性) 470(窗口命中附加异常)
//   消除成功族   945/1224/913/1124(消回合成功) 485/905/1220/1199(消强/吸取成功)
//   必暴必中     81(下N回合必中，681 半边) 582(未击败→下N回合必暴)
//   增伤/暴击附加 1048(对手异常→额外增伤) 1356(暴击→附加最大体力1/N百分比伤害)
//
// （本批家族的"消除成功"分支沿用批 6 定型的 BreakResult::SUCCESS 形状；
//   "下N回合"用 register_next_rounds_effect、"N回合内"用 register_round_effect，
//   见 round_window.cpp 批 7 头注释。）
#include "common.h"

namespace {

// ── 本批内部 helper：注册"只在当前回合跑一次"的桥接节点 ──
// 与 register_round_effect 的差别在**过期锚点**：桥（如 582/1356 的 AFTER_ACTION 判定）
// 必须锚 ctx->roundCount（跑完本回合即过期）。走 register_round_effect 的话后手方
// start=R+1 → isExpired 在 R、R+1 都为假 → 桥下一回合再跑一次，窗口被重复开。
inline void register_once_this_round(BattleContext* ctx, State trigger, int owner, int effect_id,
                                     std::vector<int> ints, EffectFn fn) {
    if (!ctx || owner < 0 || owner > 1 || !fn) {
        return;
    }
    Effect e(effect_id, 0, owner, /*rounds=*/1, EffectArgs(std::move(ints)), fn);
    auto ce = std::make_unique<ContinuousEffect>(e, trigger, owner, /*duration=*/1,
                                                 ctx->roundCount);
    ce->source_id_ = effect_id;
    ctx->register_skill_effect(trigger, owner, std::move(ce));
}

// ── "处于{status}异常"判定（与 BattleContext::has_active_abnormal_status 同式）──
// 该函数是内核符号、插件不可链接 → 用公开的 abnormal_status_end_round 数组自判
//（调用方先做 is_valid_abnormal_status_id 越界把关）。
inline bool has_anomaly_active(BattleContext* ctx, int side, int status_id) {
    return ctx && side >= 0 && side <= 1 && status_id >= 0
        && status_id < kOfficialAbnormalStatusSlotCount
        && ctx->roundCount < ctx->abnormal_status_end_round[side][status_id];
}

// ════════════════════════════════════════════════════════════════════
// 威力视图族（一次性 SKILL_EFFECT：写入视图即置"变威力"标记 → 标准变威力重算）
// ════════════════════════════════════════════════════════════════════

// ── effect 37 —— 自身HP小于1/{0}时威力为{1}倍（惊雷切 10168 等，92 技）──
// Args=[owner,target,denom,mult]。"HP 小于"= 当前体力 < 最大体力/denom（严格小于）。
EffectResult effect_37_low_hp_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int mult = args.int_args[3];
    if (self < 0 || self > 1 || denom <= 0 || mult <= 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp < max_hp_of(ctx, self) / denom) {
        auto& view = ctx->ws.skill_power_view[self];
        if (view.value > 0) {
            view = view.value * mult;
        }
    }
    return EffectResult::kOk;
}

// ── effect 645 —— 体力低于1/{0}时威力{1}倍（玄铁重盾 16330 等，52 技）──
// 与 37 同判据（"低于"同样是严格 <）；独立 id 独立注册，模板行分开便于审计。
EffectResult effect_645_low_hp_power(BattleContext* ctx, const EffectArgs& args) {
    return effect_37_low_hp_power(ctx, args);
}

// ── effect 88 —— {0}%概率伤害为{1}倍（雷神天明闪 10825 [10,4] 等，55 技）──
// Args=[owner,target,chance,mult]。概率门在生效时刻掷一次（未中=本次无倍率）；
// 概率 0/100 短路保证"不需掷就不消耗 rand"。
EffectResult effect_88_chance_power(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int chance = args.int_args[2];
    const int mult = args.int_args[3];
    if (self < 0 || self > 1 || mult <= 1) {
        return EffectResult::kOk;
    }
    if (!roll_skill_percent(*ctx, 88, 0, self, args.int_args[1], chance)) {
        return EffectResult::kOk;
    }
    auto& view = ctx->ws.skill_power_view[self];
    if (view.value > 0) {
        view = view.value * mult;
    }
    return EffectResult::kOk;
}

// ── effect 925 —— 若自身为满体力则技能威力提升{0}%（月上风华 19857 等，52 技）──
// Args=[owner,target,pct]。提升 = 威力 ×(100+pct)/100（"提升%"是乘算增量，不是固定加值）。
EffectResult effect_925_full_hp_power_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || pct <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp >= max_hp_of(ctx, self)) {
        auto& view = ctx->ws.skill_power_view[self];
        if (view.value > 0) {
            view = view.value * (100 + pct) / 100;
        }
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 回复 / 条件固伤族
// ════════════════════════════════════════════════════════════════════

// ── effect 466 —— 恢复{0}点体力（卜力回血泡 14293 等，60 技）──
// Args=[owner,target,amount]。走 heal_amount 原语（钳满/吃封回血/恢复修正）。
EffectResult effect_466_heal_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    g_core->heal_amount(ctx, owner, amount);
    return EffectResult::kOk;
}

// ── effect 467 —— 若对手处于{0}状态则附加{1}点固定伤害（炎火击 14049 [2,200] 等，58 技）──
// Args=[owner,target,status,amount]。{0}=battle_effects 异常码（2=烧伤、5=冻伤…）。
// "处于{0}状态"= 该异常槽 end_round 未到期（has_active_abnormal_status，权威判据）。
EffectResult effect_467_status_conditional_fixed(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int status = args.int_args[2];
    const int amount = args.int_args[3];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0
        || !is_valid_abnormal_status_id(status)) {
        return EffectResult::kOk;
    }
    if (!has_anomaly_active(ctx, target, status)) {
        return EffectResult::kOk;
    }
    g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ── effect 1084 —— 附加{0}点固定伤害，对手处于异常状态时固定伤害翻倍
//                    （混乱魔盾 25840 等，55 技）──
// Args=[owner,target,amount]。"处于异常状态"= 任一异常槽未到期（1001 同判据，
// has_any_active_abnormal_status 是它的 battleContext 内联版）。
EffectResult effect_1084_fixed_double_on_anomaly(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int amount = args.int_args[2];
    if (owner < 0 || owner > 1 || target < 0 || target > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    const int final_amount = ctx->has_any_active_abnormal_status(target) ? amount * 2 : amount;
    g_core->deal_pink_damage(ctx, target, final_amount, DamageKind::FIXED, owner);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 减伤窗口族
// ════════════════════════════════════════════════════════════════════

// ── effect 463 —— {0}回合内每回合所受的伤害减少{1}点（玄铁天雷盾 13998 等，65 技）──
// 与 408 逐字同族（模板只差一个"的"）→ 408 同实现：管线 REDUCE_FLAT + 窗口刷新，
// 窗口起止存 plugin_storage（键按本 effect id 分开，不与 408 互刷）。
EffectResult effect_463_damage_reduce(BattleContext* ctx, const EffectArgs& args) {
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
    const int key = 463000 + self;
    const int start = ctx->round_effect_start_round(self, rounds, EffectWindowKind::InRounds);
    auto win = std::make_shared<Win>(start, start + rounds);
    if (ctx->plugin_storage.count(key)) {
        *std::any_cast<std::shared_ptr<Win>>(ctx->plugin_storage[key]) = *win;
        return EffectResult::kOk;   // 重复使用=刷新窗口，不叠加
    }
    ctx->plugin_storage[key] = win;
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT,
        self,
        DamageEffectCategory::MITIGATE,
        [self, reduce, win](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            const int rc = c->roundCount;
            if (rc < win->first || rc >= win->second) {
                return;   // 窗口外
            }
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
                return;   // 只减"所受的攻击伤害"（红伤）
            }
            damage.final = std::max(0, damage.final - reduce);
        });
    return EffectResult::kOk;
}

// ── effect 54 —— {0}回合使对方攻击伤害是正常状态下的1/{1}倍（绿光波 10240 [1,2] 等，70 技）──
// Args=[owner,target,rounds,divisor]。乘算减伤：减伤比例 = 1 - 1/divisor（÷2 → 50%），
// 走 ws.damage_reduce_mul 槽（REDUCE_PCT 阶段乘算连乘， Calculation::applyDamageReduction）。
// in_rounds 首回合生效套路：立即写当回合 + ROUND_START 窗口 tick 重写（ws 每回合 reset）。
// 抢第一个空槽：0 = 无减伤，首个 0 槽即空位。
EffectResult effect_54_damage_fraction_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int pct = args.int_args[2];   // 减伤百分比（÷2 → 50）
    int(&slots)[4] = ctx->ws.damage_reduce_mul[self];
    for (int i = 0; i < 4; ++i) {
        if (slots[i] == 0) {
            slots[i] = pct;
            break;
        }
    }
    return EffectResult::kOk;
}

EffectResult effect_54_damage_fraction(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int divisor = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || divisor <= 1) {
        return EffectResult::kOk;
    }
    const int pct = 100 - 100 / divisor;   // 1/divisor 倍 = 减伤 (1 - 1/divisor)×100%
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    effect_54_damage_fraction_tick(ctx, EffectArgs(std::vector<int>{self, 1 - self, pct}));
    register_round_effect(ctx, State::BATTLE_ROUND_START, self, 54, rounds,
                          {self, 1 - self, pct}, &effect_54_damage_fraction_tick);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 回合窗口循环族（532/597/707 的低血条件变体；AFTER_ACTION tick + used_skill 门）
// ════════════════════════════════════════════════════════════════════

// ── effect 927 —— {0}回合内每回合使用技能则出手流程结束后恢复自身最大体力的1/{1}，
//     恢复体力时若自身体力低于最大体力的1/{2}则附加给对手等量百分比伤害
//     （电闪天地 24869 等，79 技）──
// Args=[owner,target,rounds,denom,low_denom]。低血判定在**回血前**求值（707 同款）；
// "等量"= 实际回到的血（532 口径：满血/封回血 → 回 0 → 无伤害）。
EffectResult effect_927_heal_low_pink_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    const bool low = ctx->getPet(self).hp < max_hp / std::max(1, low_denom);
    const int healed = heal_max_hp_fraction(ctx, self, denom);
    if (low && healed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

EffectResult effect_927_heal_low_pink(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 927, rounds,
                              {self, opp, denom, low_denom}, &effect_927_heal_low_pink_tick);
    }
    return EffectResult::kOk;
}

// ── effect 1065 —— {0}回合内每回合使用技能恢复自身最大体力的1/{1}并造成等量百分比伤害，
//     恢复体力时若自身体力低于最大体力的1/{2}则恢复效果和百分比伤害翻倍
//     （不动之天 25171 等，89 技）──
// Args=[owner,target,rounds,denom,low_denom]。"翻倍"作用在**本次回血量**上（钳满照常）；
// 粉伤等量**实际**回到的血（翻倍后的）。
EffectResult effect_1065_heal_pink_double_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
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
    const int before = ctx->getPet(self).hp;
    g_core->heal_amount(ctx, self, low ? base * 2 : base);
    const int healed = ctx->getPet(self).hp - before;
    if (healed > 0 && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

EffectResult effect_1065_heal_pink_double(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1065, rounds,
                              {self, opp, denom, low_denom}, &effect_1065_heal_pink_double_tick);
    }
    return EffectResult::kOk;
}

// ── effect 1067 —— {0}回合内每回合使用技能恢复自身最大体力的1/{1}，
//     恢复体力时若自身体力低于最大体力的1/{2}则恢复效果转变为吸取对手最大体力的1/{3}
//     （断善修恶 25175 等，71 技）──
// Args=[owner,target,rounds,denom,low_denom,drain_denom]。低血 → 整段转为 drain_hp
// （对手掉血、自身回等量；597 同原语）。
EffectResult effect_1067_heal_or_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int low_denom = args.int_args[3];
    const int drain_denom = args.int_args[4];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const int max_hp = max_hp_of(ctx, self);
    if (ctx->getPet(self).hp < max_hp / std::max(1, low_denom)) {
        if (g_core && g_core->drain_hp) {
            g_core->drain_hp(ctx, /*actor=*/self, /*target=*/opp, drain_denom);
        }
        return EffectResult::kOk;
    }
    heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

EffectResult effect_1067_heal_or_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    const int drain_denom = args.int_args[5];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || denom <= 0
        || drain_denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1067, rounds,
                              {self, opp, denom, low_denom, drain_denom},
                              &effect_1067_heal_or_drain_tick);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// PP / 属性窃取族
// ════════════════════════════════════════════════════════════════════

// ── effect 583 —— 使对手随机{0}项技能的PP值归零（天马彗星破 16047 等，76 技）──
// Args=[owner,target,count]。随机**不重复**；只挑 pp>0 的槽（无限 PP（pp<0）不动）。
// ★ 走 pp_zero_slot 专用原语（2026-09-23：清 PP 必须走原语——效果会监听 PP 清除事件，
//   直写 skills[i].pp 绕过事件）。
EffectResult effect_583_random_pp_zero(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->pp_zero_slot) {
        return EffectResult::kOk;
    }
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    if (opp < 0 || opp > 1 || count <= 0) {
        return EffectResult::kOk;
    }
    ElfPet& opp_pet = ctx->getPet(opp);
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(opp_pet.skills.size()); ++i) {
        if (opp_pet.skills[static_cast<std::size_t>(i)].pp > 0) {
            candidates.push_back(i);
        }
    }
    for (int k = 0; k < count && !candidates.empty(); ++k) {
        const int pick = choose_skill_range(*ctx, 583, 0, 1 - opp, opp, 0, static_cast<int>(candidates.size()) - 1);
        (void)g_core->pp_zero_slot(ctx, opp, candidates[static_cast<std::size_t>(pick)], /*actor=*/1 - opp);
        candidates.erase(candidates.begin() + pick);
    }
    return EffectResult::kOk;
}

// ── effect 444 —— 降低对手所有PP一点，并恢复自身所有PP一点（贪婪射线 13674 等，70 技）──
// Args=[owner,target]。**两个独立子句**（2026-09-23 与 425 同一拆法）：降低 = pp_reduce
// 原语（吃事件）；恢复 = 每槽 +1 钳 maxPP（PP 无恢复原语 → 直写，87 先例），不依赖对手
// 半边的结果。
EffectResult effect_444_pp_shift(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->pp_reduce) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    g_core->pp_reduce(ctx, opp, /*amount=*/1);   // ① 降低对手所有 PP 一点（走原语/发事件）
    for (Skills& self_sk : ctx->getPet(owner).skills) {   // ② 恢复自身所有 PP 一点
        if (self_sk.pp >= 0 && self_sk.pp < self_sk.maxPP) {
            self_sk.pp += 1;
        }
    }
    return EffectResult::kOk;
}

// ── effect 425 —— 随机吸取对手{0}项属性{1}，并将该属性附加给自己
//                    （死亡轰炸 17141 等，76 技）──
// Args=[owner,target,count,levels]。{1} 在数据里是负数（[-1,-2]）→ 取绝对值 = 级数。
// 池 = 全部能力等级槽（含命中，2026-09-18 用户口径"提到等级提升下降一律指含命中的 6 种"）。
// ★ **两个独立动作**（用户 2026-09-23 口径，同吸血的"一个回复一个粉伤"拆法）：
//   ① 附加下降 = stat_drop 对手（吃免弱/钳 -6）；② 附加提升 = stat_change 自己
//   （无任何检查——对手被免弱挡下**不影响**自己照常提升，"说吸多少就是多少"）。
EffectResult effect_425_random_stat_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    const int levels = std::abs(args.int_args[3]);
    if (owner < 0 || owner > 1 || opp < 0 || opp > 1 || count <= 0 || levels <= 0
        || !g_core->stat_drop || !g_core->stat_change) {
        return EffectResult::kOk;
    }
    std::vector<int> pool;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        pool.push_back(i);
    }
    for (int k = 0; k < count && !pool.empty(); ++k) {
        const int pick = choose_skill_range(*ctx, 425, 0, owner, opp, 0, static_cast<int>(pool.size()) - 1);
        const int stat = pool[static_cast<std::size_t>(pick)];
        pool.erase(pool.begin() + pick);
        g_core->stat_drop(ctx, opp, stat, levels, owner);      // ① 附加下降（免弱可挡）
        g_core->stat_change(ctx, owner, stat, levels);         // ② 附加提升（独立、必成）
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 免疫 / 封属 / 窗口命中族
// ════════════════════════════════════════════════════════════════════

// ── effect 570 —— 免疫下{0}次对手的攻击（闪光次元击 15648 等，70 技）──
// Args=[owner,target,counts]。真次数免伤票：ImmunityType::DAMAGE + counts——内核
// 默认 BLOCK 条目的注释原文就是这条语义（"免疫下N次攻击伤害"，命中即扣、扣完注销）。
EffectResult effect_570_count_attack_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int counts = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::DAMAGE, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/570,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/570);
    return EffectResult::kOk;
}

// ── effect 980 —— 先出手时对手当回合属性技能无效（雷裂残阳 19729 等，56 技）──
// Args=[owner,target]（args_num=0）。一次性：SKILL_EFFECT 桶按行动方执行，
// 能进 FIRST 侧说明本回合先手（52 同理，不查先手权）；封 1 次持续 1 回合，
// 参数对齐 478（"属性技能无效"同语义模板：attribute-only、hit_invalid=false、可穿）。
EffectResult effect_980_first_seal_attribute(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || !acting_side_is_first(ctx)) {
        return EffectResult::kOk;   // 后手无事发生
    }
    g_core->seal_skill(ctx, /*source=*/self, /*target=*/opp, /*effect_id=*/980,
                       /*attribute=*/true, /*attack=*/false,
                       /*count=*/1, /*duration_rounds=*/1, /*penetrable=*/true,
                       /*source_slot=*/-1, EffectScope::ON_STAGE, /*hit_invalid=*/false,
                       /*chance_pct=*/100, /*consumed_when_pierced=*/false);
    return EffectResult::kOk;
}

// ── effect 470 —— {0}回合内若自身攻击技能命中则{1}%令对手{2}
//                    （单骑万侯斩 16566 [3,50,8] 等，85 技）──
// Args=[owner,target,rounds,chance,anomaly]。
// 命中门控是**结构性的**：ON_SKILL_HIT 桶只在 skill_exec_result != SKILL_INVALID 时执行
// （battleFsm：miss/被盔连桶带钩一起跳过）。本回合（当次攻击）由 handler 直接判
// ——先手窗口起点含当回合，但 ON_SKILL_HIT 已过，所以立即补一次（426 "立即写+tick" 双写套路）；
// 后续回合走窗口 tick。
EffectResult effect_470_hit_anomaly_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (!executed_skill_is_attack(ctx, self)
        || !roll_skill_percent(*ctx, 470, 0, self, opp, chance)) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, opp, anomaly, random_anomaly_duration(), self);
    return EffectResult::kOk;
}

EffectResult effect_470_hit_anomaly_window(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int chance = args.int_args[3];
    const int anomaly = args.int_args[4];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    // 当次攻击（命中才会走到 SKILL_EFFECT）：攻击技能 + 掷点 → 立即施加
    effect_470_hit_anomaly_tick(ctx, EffectArgs(std::vector<int>{self, opp, chance, anomaly}));
    // 后续回合：窗口 tick 挂 ON_SKILL_HIT（结构性命中门控 + 每回合"使用了攻击技能"判定）
    for (State st : {State::BATTLE_FIRST_ON_SKILL_HIT, State::BATTLE_SECOND_ON_SKILL_HIT}) {
        register_round_effect(ctx, st, self, 470, rounds,
                              {self, opp, chance, anomaly}, &effect_470_hit_anomaly_tick);
    }
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 消除成功族（批 6 形状：SUCCESS 才执行子句；NO_EFFECTS/IMMUNE 都不算成功）
// ════════════════════════════════════════════════════════════════════

// 共用 tick：ROUND_START 写必暴（58 同款；暴击判定只对攻击技能跑，无需再限定）。
EffectResult effect_batch8_must_crit_tick(BattleContext* ctx, const EffectArgs& args) {
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

// ── effect 945 —— 消除对手回合类效果，消除成功则恢复自身全部体力（猛断凛威斩等，60 技）──
EffectResult effect_945_break_then_full_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->break_round_effects || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    g_core->heal_amount(ctx, self, max_hp_of(ctx, self));
    return EffectResult::kOk;
}

// ── effect 1224 —— 消除对手回合类效果，消除成功则使自身下{0}回合必定致命一击
//                     （炼·陆离 28730 等，62 技）──
// Args=[owner,target,rounds]。"下N回合" → register_next_rounds_effect（本回合不算）。
EffectResult effect_1224_break_then_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1224, rounds,
                                {self, 1 - self}, &effect_batch8_must_crit_tick);
    return EffectResult::kOk;
}

// 共用 tick：ROUND_START 写"额外增伤"（非通用 = 乘法通道，AMP_EXTRA；693 同款）。
EffectResult effect_batch8_amp_extra_tick(BattleContext* ctx, const EffectArgs& args) {
    return effect_set_damage_amp_extra(ctx, args);
}

// ── effect 913 —— 消除对手回合类效果，消除成功则下回合自身造成的攻击伤害额外提升{0}%
//                    （风电惊世爆 19481 等，62 技）──
// Args=[owner,target,pct]。"下回合"= 1 回合的 NextRounds 窗口。
EffectResult effect_913_break_then_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 913, /*rounds=*/1,
                                {self, 1 - self, pct}, &effect_batch8_amp_extra_tick);
    return EffectResult::kOk;
}

// ── effect 1124 —— 消除对手回合类效果，消除成功则自身下{0}回合造成的攻击伤害额外提升{1}%
//                     （钢熠身躯 28944 等，52 技）──
// Args=[owner,target,rounds,pct]。
EffectResult effect_1124_break_then_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core
        || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int pct = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || pct == 0) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, opp) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1124, rounds,
                                {self, 1 - self, pct}, &effect_batch8_amp_extra_tick);
    return EffectResult::kOk;
}

// ── effect 485 —— 消除对手能力提升状态，消除成功则恢复自身全部体力（断空光牙剑等，51 技）──
// "消强成功" = clear_stat_boosts 返回清掉个数 > 0（被 STAT_CLEAR 免疫挡下 → 0 → 不算成功）。
EffectResult effect_485_clear_then_full_heal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->clear_stat_boosts || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }
    g_core->heal_amount(ctx, self, max_hp_of(ctx, self));
    return EffectResult::kOk;
}

// ── effect 905 —— 消除对手能力提升，消除成功{0}回合内对手无法通过自身技能恢复体力
//                    （朽梦挽歌 19474 等，51 技）──
// Args=[owner,target,rounds]。封回血 = HEAL_BLOCK 票挂对手（797 同款）。
EffectResult effect_905_clear_then_heal_block(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/905,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/905);
    return EffectResult::kOk;
}

// ── effect 1220 —— 消除对手能力提升状态，消除成功则下{0}回合自身攻击必定致命一击
//                     （天罗幻象 31026 等，59 技）──
EffectResult effect_1220_clear_then_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->clear_stat_boosts) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts(ctx, opp) <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 1220, rounds,
                                {self, 1 - self}, &effect_batch8_must_crit_tick);
    return EffectResult::kOk;
}

// ── effect 1199 —— 吸取对手能力提升状态，吸取成功则对手{0}回合内无法通过自身技能恢复体力
//                     （聚能破穹光 30891 等，53 技）──
// "吸取成功" = transfer_stat_boosts 转移个数 > 0（85/677 同原语；对手有 STAT_CLEAR
// 保护时转移失败 → 不给封回血）。
EffectResult effect_1199_drain_then_heal_block(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->transfer_stat_boosts) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (g_core->transfer_stat_boosts(ctx, /*from=*/opp, /*to=*/self) <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(opp, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/rounds, /*source_id=*/1199,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/0, /*source_effect_id=*/1199);
    return EffectResult::kOk;
}

// ════════════════════════════════════════════════════════════════════
// 必中 / 条件必暴 / 增伤 / 暴击附加
// ════════════════════════════════════════════════════════════════════

// ── effect 81 —— 下{0}回合自身攻击技能必定命中（追踪魔影 11775 等，52 技）──
// 681 的必中半边：MOVE_RIGHT 窗口 tick 写 ws.must_hit_grant（凭证物化在 query_usage，
// 晚于本时点）+"攻击技能"限定（属性技能不白拿必中）。
EffectResult effect_81_must_hit_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1 || !selected_skill_is_attack(ctx, self)) {
        return EffectResult::kOk;
    }
    ctx->ws.must_hit_grant[self] = true;
    return EffectResult::kOk;
}

EffectResult effect_81_next_rounds_must_hit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, 81, rounds,
                                {self, 1 - self}, &effect_81_must_hit_tick);
    return EffectResult::kOk;
}

// ── effect 582 —— 若未击败对手则下{0}回合攻击必定致命一击（焚神炽烈斩 15987 等，88 技）──
// Args=[owner,target,rounds]。"未击败"要在**伤害落地后**判定（1307 同款理由），而本效果
// 默认注册在 SKILL_EFFECT（伤害前）→ AFTER_ACTION 桥：桥 tick 判对手存活，才开必暴窗口。
EffectResult effect_582_bridge_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 本条不给
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 582, rounds,
                                {self, 1 - self}, &effect_batch8_must_crit_tick);
    return EffectResult::kOk;
}

EffectResult effect_582_undeathed_must_crit(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0) {
        return EffectResult::kOk;
    }
    // FIRST/SECOND 各注册一条，只有行动方那条会跑 → 恰好一次（532 同款双注册）。
    // 桥必须锚当前回合过期（见 register_once_this_round 注释），不能走 round_effect_start_round。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_once_this_round(ctx, st, self, 582, {self, opp, rounds},
                                 &effect_582_bridge_tick);
    }
    return EffectResult::kOk;
}

// ── effect 1048 —— 若对手处于异常状态则自身造成的攻击伤害额外提升{0}%
//                     （无极天影破 19994 等，95 技）──
// Args=[owner,target,pct]。"额外提升"= 非通用增伤 → damage_add_extra_mul（乘法通道，
// AMP_EXTRA 阶段，693 同款）；一次性写本回合 ws（本技能的攻击吃得到，管线在 SKILL_EFFECT 后跑）。
EffectResult effect_1048_anomaly_damage_up(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    if (!ctx->has_any_active_abnormal_status(opp)) {
        return EffectResult::kOk;
    }
    return effect_set_damage_amp_extra(ctx, args);
}

// ── effect 1356 —— 打出致命一击则附加对手最大体力1/{0}的百分比伤害
//                     （X·深紫彗星 32233 等，52 技）──
// Args=[owner,target,denom]。暴击与伤害都在结算后可知 → AFTER_ACTION 桥读
// resolvedDamage.isCrit（免爆在 BLOCK 阶段读的是同一位）；百分比伤害走 PERCENT_VALUE
// （532 同款：具体值、百分比档）。
EffectResult effect_1356_crit_percent_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const DamageSnapshot& damage = ctx->resolvedDamage;
    if (!damage.isCrit || damage.attackerId != self || damage.defenderId != opp
        || damage.final <= 0) {
        return EffectResult::kOk;   // 没暴击 / 不是我的攻击
    }
    g_core->deal_pink_damage(ctx, opp, max_hp_of(ctx, opp) / denom, DamageKind::PERCENT_VALUE,
                             self);
    return EffectResult::kOk;
}

EffectResult effect_1356_crit_percent_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_once_this_round(ctx, st, self, 1356, {self, opp, denom},
                                 &effect_1356_crit_percent_tick);
    }
    return EffectResult::kOk;
}

// ── effect 849 —— {0}回合内每回合使用技能则造成伤害前随机吸取对手{1}项能力值-1
//                   （万世一剑寒 19449 等，73 技）──
// Args=[owner,target,rounds,count]。"造成伤害前"= BEFORE_SKILL_HIT（本轮出手链最早的
// 技能时点，先手权已结算、伤害未算）；每回合 tick + used_skill 门。仅降对手（文本无
// "附加给自己"，区别于 425）；走 stat_drop（查免弱、钳 -6）。
EffectResult effect_849_pre_damage_stat_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->stat_drop) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int count = args.int_args[2];
    if (!used_skill_this_round(ctx, self) || count <= 0) {
        return EffectResult::kOk;
    }
    std::vector<int> pool;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        pool.push_back(i);
    }
    for (int k = 0; k < count && !pool.empty(); ++k) {
        const int pick = choose_skill_range(*ctx, 849, 0, self, opp, 0, static_cast<int>(pool.size()) - 1);
        const int stat = pool[static_cast<std::size_t>(pick)];
        pool.erase(pool.begin() + pick);
        g_core->stat_drop(ctx, opp, stat, 1, self);
    }
    return EffectResult::kOk;
}

EffectResult effect_849_pre_damage_stat_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int count = args.int_args[3];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || rounds <= 0 || count <= 0) {
        return EffectResult::kOk;
    }
    // 当次攻击：SKILL_EFFECT 跑在伤害重算/管线之前，此刻降的等级本次伤害吃得到
    //（"造成伤害前"的当回合补位；窗口 tick 只覆盖后续回合——BEFORE_SKILL_HIT 已过）。
    effect_849_pre_damage_stat_drain_tick(ctx, EffectArgs(std::vector<int>{self, opp, count}));
    for (State st : {State::BATTLE_FIRST_BEFORE_SKILL_HIT, State::BATTLE_SECOND_BEFORE_SKILL_HIT}) {
        register_round_effect(ctx, st, self, 849, rounds,
                              {self, opp, count}, &effect_849_pre_damage_stat_drain_tick);
    }
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    // 威力视图族
    SKILL_EFFECT(37, effect_37_low_hp_power);            // 自身HP小于1/N时威力M倍
    SKILL_EFFECT(645, effect_645_low_hp_power);          // 体力低于1/N时威力M倍
    SKILL_EFFECT(88, effect_88_chance_power);            // P%概率伤害为M倍
    SKILL_EFFECT(925, effect_925_full_hp_power_up);      // 满体力威力提升P%
    // 回复/固伤
    SKILL_EFFECT(466, effect_466_heal_fixed);            // 恢复N点体力
    SKILL_EFFECT(467, effect_467_status_conditional_fixed); // 对手处于X状态附加固伤
    SKILL_EFFECT(1084, effect_1084_fixed_double_on_anomaly); // 固伤,对手异常翻倍
    // 减伤窗口
    SKILL_EFFECT(463, effect_463_damage_reduce);         // N回合内每回合所受伤害减M点(408同族)
    SKILL_EFFECT(54, effect_54_damage_fraction);         // N回合对方攻击伤害1/M倍(乘算减伤槽)
    // 回合窗口循环族
    SKILL_EFFECT(927, effect_927_heal_low_pink);         // 回血,低血附加等量百分比伤害
    SKILL_EFFECT(1065, effect_1065_heal_pink_double);    // 回血+等量百分比,低血双翻倍
    SKILL_EFFECT(1067, effect_1067_heal_or_drain);       // 回血,低血转为吸取
    // PP/属性窃取
    SKILL_EFFECT(583, effect_583_random_pp_zero);        // 随机N项技能PP归零
    SKILL_EFFECT(444, effect_444_pp_shift);              // 对手全PP-1自身+1(叵口径)
    SKILL_EFFECT(425, effect_425_random_stat_drain);     // 随机吸取N项属性并附加给自己
    // 免疫/封属/窗口命中
    SKILL_EFFECT(570, effect_570_count_attack_immunity); // 免疫下N次对手的攻击(次数免伤票)
    SKILL_EFFECT(980, effect_980_first_seal_attribute);  // 先手当回合封对手属性技能
    SKILL_EFFECT(470, effect_470_hit_anomaly_window);    // N回合内攻击命中P%附加异常
    // 消除成功族
    SKILL_EFFECT(945, effect_945_break_then_full_heal);  // 消回合成功→回满血
    SKILL_EFFECT(1224, effect_1224_break_then_must_crit); // 消回合成功→下N回合必暴
    SKILL_EFFECT(913, effect_913_break_then_damage_up);  // 消回合成功→下回合额外增伤
    SKILL_EFFECT(1124, effect_1124_break_then_damage_up); // 消回合成功→下N回合额外增伤
    SKILL_EFFECT(485, effect_485_clear_then_full_heal);  // 消强成功→回满血
    SKILL_EFFECT(905, effect_905_clear_then_heal_block); // 消强成功→N回合封对手回血
    SKILL_EFFECT(1220, effect_1220_clear_then_must_crit); // 消强成功→下N回合必暴
    SKILL_EFFECT(1199, effect_1199_drain_then_heal_block); // 吸取成功→N回合封对手回血
    // 必中/条件必暴/增伤/暴击附加
    SKILL_EFFECT(81, effect_81_next_rounds_must_hit);    // 下N回合攻击技能必中
    SKILL_EFFECT(582, effect_582_undeathed_must_crit);   // 未击败→下N回合必暴(AFTER_ACTION桥)
    SKILL_EFFECT(1048, effect_1048_anomaly_damage_up);   // 对手异常→本次攻击额外增伤
    SKILL_EFFECT(1356, effect_1356_crit_percent_damage); // 暴击→附加最大体力1/N百分比伤害
    SKILL_EFFECT(849, effect_849_pre_damage_stat_drain); // N回合内出手前随机降对手N项能力

}  // namespace
