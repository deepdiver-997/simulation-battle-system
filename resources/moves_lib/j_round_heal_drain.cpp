// J 组 J6 · 每回合用技恢复/吸取窗族 —— 12 宏/136 载体（2026-10-01，worktree ../agents/j6）
//
// 家族语义（effect_info 模板即权威）："{0}回合内每回合使用技能 <恢复/吸取>[条件变体]"。
// **无门槛**（不像 J2 消强/J4 吸取族——纯窗口循环，不需要预置条件）。
// 落点 = H8 批 5/2 范式逐字（round_window.cpp 532/597/707 已实现直系）：
//   FIRST/SECOND_AFTER_ACTION 双侧注册（InRounds rounds={0}）+ tick 里
//   used_skill_this_round 门（"每回合**使用技能**"——没出招不结算；CHOOSE 先于
//   AFTER_ACTION，roundChoice 届时已写入）+ heal_amount/drain_hp/fixed_damage/
//   deal_pink_damage 原语。等量口径 = **实际回到的血**（532：满血/封回血 → 回 0 →
//   无伤害）。首回合当次即结算（AFTER_ACTION 晚于 SKILL_EFFECT 注册点）。
//
// 条件变体逐效果：
//   791  回 1/{1}，回血时自身 HP<对手 HP → 翻倍（707 的低体条件换成对手比较）
//   1875 回 1/{1}+等量粉伤，自身 HP<最大体力1/{2} → 恢复翻倍（707+532 合体）
//   1924 回 {1} 点+等量固定伤害（等量=实际恢复量，1924 布局 [rounds,points]）
//   1695 回 1/{1}+等量粉伤；满体力 → 转变为附加自身最大体力1/{2}粉伤（不回血）
//   1602 回 1/{1}；满体力 → 恢复己方所有不在场精灵 {2} 点（阵亡不复活、消逝跳过、
//        在场跳过；直写 hp 钳各自上限——无场下治疗原语，首例记档）
//   1842 吸对手 {1} 点，自身 HP<对手 HP → 提升至 {2} 点（点数吸血 = 2274 口径：
//        fixed_damage + 按实际扣减回血）
//   1983 吸对手 {1} 点
//   1836 吸对手最大体力1/{1}（597 原语 drain_hp）；自身 HP<最大体力1/{2} → 转变为
//        附加**翻倍吸取量**的粉伤（不吸取）
//   2021 回 {1} 点；自身 HP<最大体力1/{2} → 转变为吸取对手 {3} 点（不恢复）
//   1839 回 1/{1}（用技窗）＋ 每回合**结束后**若未击败对手 → 吸对手最大体力1/{2}
//        （第二段独立触发=OLD_ROUND_END_1 窗、无用技门、opp 存活门——字面语序记档）
//   2007 回 1/{1}（用技窗）＋ 每回合结束后若对手处于能力提升状态 → 吸对手 {2} 点且
//        为**对手**附加等量护盾（shield_bank_.add，1351 同 API；"等量"=实际吸到量）
//   1785 吸对手最大体力1/{1}；自身满体力 → 转变为恢复己方所有不在场精灵 {2} 点（不吸取）
//
// ⚠️ 窗口口径："{0}回合内" = InRounds 含当回合（首回合 AFTER_ACTION 当次即结算，
//    532/597/707 已背书）；1839/2007 第二段的"每回合结束后" = OLD_ROUND_END_1
//    （execute_registered_actions(-1) → 双侧桶都跑，registered_round 同窗口）。
//
// 载体切片双源核对（unity 库 json_each 精确匹配；邻居 args_num 逐个核对）：
//   791  剑道莫寻 24660 [585×6,48,791]|[1×6,4,4,3] → 791=[4,3]
//   1875 冥途誓约 28201 [1875]|[4,3,2]（单载体）
//   1924 炫影仙尘踏 36568 [1924,713,1662]|[4,300,50] → 1924=[4,300]
//   1695 朦月龙吟 27613 [191,1695,440]|[4,4,3,4,3,3] → 1695=[4,3,4]
//   1602 树灵滋养 27364 [911,1602,1026]|[1,4,3,100,2,2] → 1602=[4,3,100]
//   1842 冥咒妒命杀 36534 [1842,1040,1049]|[3,280,350,100,28,1,1] → 1842=[3,280,350]
//   1983 良址招租 37542 [1983]|[6,233]（单载体）
//   1836 鸿渐于陆 28032 [191,1836,1837,843]|[4,4,3,2,100,28,1,2,2] → 1836=[4,3,2]
//   2021 寒霜之境 28478 [993,2021,843]|[1,5,4,300,3,300,2,2] → 2021=[4,300,3,300]
//   1839 祸乱之躯 28039 [1344,1839,843]|[1,4,3,4,2,2] → 1839=[4,3,4]
//   2007 月兔祝福 38096 [2007,746]|[2,3,250,…] → 2007=[2,3,250]
//   1785 沉云浸野 28866 [911,1687,1785,2226]|[1,4,4,3,120,1] → 1785=[4,3,120]
//
// 动工前置三查（2026-10-01）：①内核 grep 无 12 id 硬编码 ②custom_effect_overrides
// 无声明（InRounds 默认口径即官方语义）③moves_lib 无既有注册——真缺口。
//
// 场景回归：test/scenario/scenario_915_round_heal_drain.cpp（号段 915-919 一文件）。

#include "common.h"

namespace {

// ── 共用小件 ──────────────────────────────────────────────────────────────

// 点数吸血（2274 口径：fixed_damage 走固伤通道，按**实际扣减**回血、回血吃封回血）。
inline int drain_points(BattleContext* ctx, int owner, int opp, int points) {
    if (!g_core || !g_core->fixed_damage || !g_core->heal_amount) {
        return 0;
    }
    const int before = ctx->getPet(opp).hp;
    (void)g_core->fixed_damage(ctx, opp, points);
    const int dealt = before - ctx->getPet(opp).hp;
    if (dealt > 0) {
        (void)g_core->heal_amount(ctx, owner, dealt);
    }
    return dealt;
}

// 场下全体治疗（1602/1785）：跳过在场、阵亡（不复活）、消逝；直写 hp 钳各自上限。
// 引擎无场下治疗原语（首例记档，见文件头）。
inline int heal_off_field_party(BattleContext* ctx, int side, int pts) {
    if (!ctx || pts <= 0) {
        return 0;
    }
    auto& pets = ctx->seerRobot[side].elfPets;
    int total = 0;
    const int n = static_cast<int>(pets.size()) < 6 ? static_cast<int>(pets.size()) : 6;
    for (int i = 0; i < n; ++i) {
        if (i == ctx->on_stage[side]) {
            continue;   // 在场的不算"不在场精灵"
        }
        ElfPet& p = pets[i];
        if (p.hp <= 0) {
            continue;   // 阵亡不复活
        }
        if (ctx->is_vanished(side, i)) {
            continue;   // 消逝不可逆
        }
        int max_hp = p.numericalBase[NumericalPropertyIndex::HP];
        if (max_hp <= 0) {
            max_hp = p.numericalProperties[NumericalPropertyIndex::HP];
        }
        if (max_hp <= 0) {
            continue;
        }
        const int before = p.hp;
        p.hp = std::min(before + pts, max_hp);
        total += p.hp - before;
    }
    return total;
}

// ── 791 —— {0}回合内每回合使用技能恢复 1/{1}，回血时自身 HP<对手 HP → 翻倍 ──
EffectResult effect_791_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const int base = max_hp_of(ctx, self) / denom;
    if (base <= 0 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const bool low = ctx->getPet(self).hp < ctx->getPet(opp).hp;
    g_core->heal_amount(ctx, self, low ? base * 2 : base);
    return EffectResult::kOk;
}

EffectResult effect_791_round_heal_low(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 791, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 791, args,
                     "791 布局=[owner,target,回合,分母]（int_count>=4）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 791, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 791, args, "需要 回合/分母 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 791, rounds, {self, opp, denom},
                              &effect_791_tick);
    }
    return EffectResult::kOk;
}

// ── 1875 —— 回 1/{1}+等量粉伤；自身 HP<最大体力1/{2} → 恢复翻倍（粉伤=实际回到的血）──
EffectResult effect_1875_tick(BattleContext* ctx, const EffectArgs& args) {
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
    const int healed = heal_max_hp_fraction(ctx, self, denom)
                           + (low ? heal_max_hp_fraction(ctx, self, denom) : 0);
    if (healed > 0 && g_core && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    (void)base;
    return EffectResult::kOk;
}

EffectResult effect_1875_round_heal_pink(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1875, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1875, args,
                     "1875 布局=[owner,target,回合,分母,低体分母]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1875,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && low_denom > 0, 1875, args,
                     "需要 回合/分母 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1875, rounds, {self, opp, denom, low_denom},
                              &effect_1875_tick);
    }
    return EffectResult::kOk;
}

// ── 1924 —— 回 {1} 点 + 等量固定伤害 ──
EffectResult effect_1924_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (!g_core || !g_core->heal_amount || points <= 0) {
        return EffectResult::kOk;
    }
    const int before = ctx->getPet(self).hp;
    g_core->heal_amount(ctx, self, points);
    const int healed = ctx->getPet(self).hp - before;
    if (healed > 0 && g_core->fixed_damage) {
        (void)g_core->fixed_damage(ctx, opp, healed);
    }
    return EffectResult::kOk;
}

EffectResult effect_1924_round_heal_fixed(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1924, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1924, args,
                     "1924 布局=[owner,target,回合,点数]（int_count>=4）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1924,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && points > 0, 1924, args, "需要 回合/点数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1924, rounds, {self, opp, points},
                              &effect_1924_tick);
    }
    return EffectResult::kOk;
}

// ── 1695 —— 回 1/{1}+等量粉伤；满体力 → 转变为附加自身最大体力1/{2}粉伤 ──
EffectResult effect_1695_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int alt_denom = args.int_args[3];
    if (!used_skill_this_round(ctx, self) || !g_core) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp >= max_hp_of(ctx, self)) {
        // 满体力 → 转变：不回血，只附加 1/{2} 粉伤
        const int max_hp = max_hp_of(ctx, self);
        if (max_hp / std::max(1, alt_denom) > 0 && g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, opp, max_hp / alt_denom, DamageKind::PERCENT_VALUE,
                                     self);
        }
        return EffectResult::kOk;
    }
    const int healed = heal_max_hp_fraction(ctx, self, denom);
    if (healed > 0 && g_core->deal_pink_damage) {
        g_core->deal_pink_damage(ctx, opp, healed, DamageKind::PERCENT_VALUE, self);
    }
    return EffectResult::kOk;
}

EffectResult effect_1695_round_heal_full_alt(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1695, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1695, args,
                     "1695 布局=[owner,target,回合,分母,转变分母]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int alt_denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1695,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && alt_denom > 0, 1695, args,
                     "需要 回合/分母 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1695, rounds, {self, opp, denom, alt_denom},
                              &effect_1695_tick);
    }
    return EffectResult::kOk;
}

// ── 1602 —— 回 1/{1}；满体力 → 恢复己方所有不在场精灵 {2} 点 ──
EffectResult effect_1602_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    const int team_pts = args.int_args[3];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp >= max_hp_of(ctx, self)) {
        heal_off_field_party(ctx, self, team_pts);
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

EffectResult effect_1602_round_heal_bench(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1602, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1602, args,
                     "1602 布局=[owner,target,回合,分母,场下点数]（int_count>=5）");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int team_pts = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1, 1602, args, "需要 owner ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && team_pts > 0, 1602, args,
                     "需要 回合/分母/点数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1602, rounds, {self, 1 - self, denom, team_pts},
                              &effect_1602_tick);
    }
    return EffectResult::kOk;
}

// ── 1842 —— 吸对手 {1} 点；自身 HP<对手 HP → 提升至 {2} 点 ──
EffectResult effect_1842_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    const int boosted = args.int_args[3];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const bool low = ctx->getPet(self).hp < ctx->getPet(opp).hp;
    (void)drain_points(ctx, self, opp, low ? boosted : points);
    return EffectResult::kOk;
}

EffectResult effect_1842_round_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1842, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1842, args,
                     "1842 布局=[owner,target,回合,点数,提升点数]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    const int boosted = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1842,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && points > 0 && boosted > 0, 1842, args,
                     "需要 回合/点数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1842, rounds, {self, opp, points, boosted},
                              &effect_1842_tick);
    }
    return EffectResult::kOk;
}

// ── 1983 —— 吸对手 {1} 点 ──
EffectResult effect_1983_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    (void)drain_points(ctx, self, opp, points);
    return EffectResult::kOk;
}

EffectResult effect_1983_round_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1983, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 1983, args,
                     "1983 布局=[owner,target,回合,点数]（int_count>=4）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1983,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && points > 0, 1983, args, "需要 回合/点数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1983, rounds, {self, opp, points},
                              &effect_1983_tick);
    }
    return EffectResult::kOk;
}

// ── 1836 —— 吸对手最大体力1/{1}；自身 HP<最大体力1/{2} → 转变为附加翻倍吸取量粉伤 ──
EffectResult effect_1836_tick(BattleContext* ctx, const EffectArgs& args) {
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
    if (base <= 0) {
        return EffectResult::kOk;
    }
    const bool low = ctx->getPet(self).hp < max_hp / std::max(1, low_denom);
    if (low) {
        // 转变：不吸取，附加翻倍吸取量的粉伤
        if (g_core && g_core->deal_pink_damage) {
            g_core->deal_pink_damage(ctx, opp, base * 2, DamageKind::PERCENT_VALUE, self);
        }
        return EffectResult::kOk;
    }
    if (g_core && g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, /*actor=*/self, /*target=*/opp, denom);
    }
    return EffectResult::kOk;
}

EffectResult effect_1836_round_drain_alt(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1836, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1836, args,
                     "1836 布局=[owner,target,回合,分母,低体分母]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int low_denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1836,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && low_denom > 0, 1836, args,
                     "需要 回合/分母 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1836, rounds, {self, opp, denom, low_denom},
                              &effect_1836_tick);
    }
    return EffectResult::kOk;
}

// ── 2021 —— 回 {1} 点；自身 HP<最大体力1/{2} → 转变为吸取对手 {3} 点 ──
EffectResult effect_2021_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int points = args.int_args[2];
    const int low_denom = args.int_args[3];
    const int alt_points = args.int_args[4];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    const bool low =
        ctx->getPet(self).hp < max_hp_of(ctx, self) / std::max(1, low_denom);
    if (low) {
        // 转变：不恢复，吸取对手 alt_points 点
        (void)drain_points(ctx, self, opp, alt_points);
        return EffectResult::kOk;
    }
    if (g_core && g_core->heal_amount && points > 0) {
        (void)g_core->heal_amount(ctx, self, points);
    }
    return EffectResult::kOk;
}

EffectResult effect_2021_round_heal_alt(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2021, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 6, 2021, args,
                     "2021 布局=[owner,target,回合,点数,低体分母,转变点数]（int_count>=6）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int points = args.int_args[3];
    const int low_denom = args.int_args[4];
    const int alt_points = args.int_args[5];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 2021,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && points > 0 && low_denom > 0 && alt_points > 0, 2021, args,
                     "需要 参数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 2021, rounds,
                              {self, opp, points, low_denom, alt_points}, &effect_2021_tick);
    }
    return EffectResult::kOk;
}

// ── 1839 —— 回 1/{1}（用技窗）＋ 每回合结束后未击败对手 → 吸对手最大体力1/{2} ──
EffectResult effect_1839_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

EffectResult effect_1839_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int drain_denom = args.int_args[2];
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // "若未击败对手"
    }
    if (g_core && g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, self, opp, drain_denom);
    }
    return EffectResult::kOk;
}

EffectResult effect_1839_round_heal_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1839, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1839, args,
                     "1839 布局=[owner,target,回合,分母,吸取分母]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int drain_denom = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1839,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && drain_denom > 0, 1839, args,
                     "需要 回合/分母 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1839, rounds, {self, opp, denom},
                              &effect_1839_heal_tick);
    }
    // 第二段：每回合结束后（OLD_ROUND_END_1 双侧桶都跑；无用技门，字面语序记档）
    register_round_effect(ctx, State::BATTLE_OLD_ROUND_END_1, self, 1839, rounds,
                          {self, opp, drain_denom}, &effect_1839_drain_tick);
    return EffectResult::kOk;
}

// ── 2007 —— 回 1/{1}（用技窗）＋ 每回合结束后对手有提升 → 吸对手 {2} 点+等量护盾 ──
EffectResult effect_2007_heal_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[2];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    (void)heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

EffectResult effect_2007_drain_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int drain_pts = args.int_args[2];
    // "对手处于能力提升状态"（六槽任一正等级；962 同判据）
    bool boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            boosted = true;
        }
    }
    if (!boosted) {
        return EffectResult::kOk;
    }
    const int dealt = drain_points(ctx, self, opp, drain_pts);
    if (dealt > 0) {
        // "为对手附加等量护盾"（等量=实际吸到量；shield_bank_，1351 同 API）
        ctx->getPet(opp).shield_bank_.add(/*priority=*/0, dealt, /*source_id=*/2007,
                                          /*duration=*/0);
    }
    return EffectResult::kOk;
}

EffectResult effect_2007_round_heal_drain(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 2007, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 2007, args,
                     "2007 布局=[owner,target,回合,分母,吸取点数]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int drain_pts = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 2007,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && drain_pts > 0, 2007, args,
                     "需要 回合/分母/点数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 2007, rounds, {self, opp, denom},
                              &effect_2007_heal_tick);
    }
    register_round_effect(ctx, State::BATTLE_OLD_ROUND_END_1, self, 2007, rounds,
                          {self, opp, drain_pts}, &effect_2007_drain_tick);
    return EffectResult::kOk;
}

// ── 1785 —— 吸对手最大体力1/{1}；自身满体力 → 转变为恢复己方所有不在场精灵 {2} 点 ──
EffectResult effect_1785_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int denom = args.int_args[2];
    const int team_pts = args.int_args[3];
    if (!used_skill_this_round(ctx, self)) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(self).hp >= max_hp_of(ctx, self)) {
        heal_off_field_party(ctx, self, team_pts);
        return EffectResult::kOk;   // 转变：不吸取
    }
    if (g_core && g_core->drain_hp) {
        (void)g_core->drain_hp(ctx, self, opp, denom);
    }
    return EffectResult::kOk;
}

EffectResult effect_1785_round_drain_bench(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 1785, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 5, 1785, args,
                     "1785 布局=[owner,target,回合,分母,场下点数]（int_count>=5）");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    const int team_pts = args.int_args[4];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && opp != self, 1785,
                     args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0 && team_pts > 0, 1785, args,
                     "需要 回合/分母/点数 > 0");
    for (const State st :
         {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        register_round_effect(ctx, st, self, 1785, rounds, {self, opp, denom, team_pts},
                              &effect_1785_tick);
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(791, effect_791_round_heal_low)
SKILL_EFFECT(1875, effect_1875_round_heal_pink)
SKILL_EFFECT(1924, effect_1924_round_heal_fixed)
SKILL_EFFECT(1695, effect_1695_round_heal_full_alt)
SKILL_EFFECT(1602, effect_1602_round_heal_bench)
SKILL_EFFECT(1842, effect_1842_round_drain)
SKILL_EFFECT(1983, effect_1983_round_drain)
SKILL_EFFECT(1836, effect_1836_round_drain_alt)
SKILL_EFFECT(2021, effect_2021_round_heal_alt)
SKILL_EFFECT(1839, effect_1839_round_heal_drain)
SKILL_EFFECT(2007, effect_2007_round_heal_drain)
SKILL_EFFECT(1785, effect_1785_round_drain_bench)
