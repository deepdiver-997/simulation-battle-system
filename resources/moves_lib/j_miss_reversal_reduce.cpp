// J152 · MISS/反转/减伤杂门 —— 177×11 / 961×10 / 522×11（2026-10-04，worktree ../agents/j152）
//
// 家族语义（effect_info 官方模板 + 载体切片核对，工单 J152 行处方）：
//
// 177（args_num=2）"{0}回合内若对手MISS则自身恢复1/{1}的最大体力值"：
//   - EVENT_SKILL_INVALID watcher 置旗（emit actor=使用方，J42 1320/J55 160 直系：
//     ev.actor==对手 + 攻击技过滤=对手所选槽位非属性技）；
//   - ROUND_END InRounds tick：有旗 → heal_max_hp_fraction(自身,{1}) + 清旗
//     （每回合至多结算一次，旗键按自身）；
// 961（args_num=1）"反转自身能力下降，反转成功则自身下{0}回合免疫攻击伤害"：
//   - stat_reversal(自身)==REVERSED 门（1194/1232 直系；NOTHING/BLOCKED 都算失败）；
//   - NextRounds({0}) ROUND_START tick：grant_immunity(自身, DAMAGE, ~0ULL,
//     duration=1)（J14 1024 免攻票同款"当回合"票，逐回合续）；
// 522（args_num=2）"{0}回合内若自身处于异常状态则受到伤害减少{1}点"：
//   - InRounds({0}) 窗 + REDUCE_FLAT/MITIGATE 回调（969/463 直系）：伤害落地时
//     判 has_active_anomaly(自身，任一 0..43) → final-={1}（只减红伤，463 家族口径）。
//
// 动工前置三查（2026-10-04，J152 行立单时已核）：①内核 grep 3 id 无行为硬编码
//   ②battle_effects/custom_effect_overrides 无行 ③moves_lib 并集无注册。
//   切片：177 [回合,分母] 11/11（纯链 21543 光佑守护 [5,2]/26485 天地灵气 [3,3]）；
//   961 零参 10/10；522 [回合,点数] 11/11（纯链 34285 初发芙蓉 [3,300]）。
// 先例：J42 1320/J55 160（MISS watcher+攻击技过滤）、J55（ROUND_START tick+旗）、
//   J15 775（heal_max_hp_fraction 回满）、stat_reversal（1257 通道）、
//   J 组 969/463（REDUCE_FLAT+MITIGATE 点数减伤窗）。
// 场景回归：test/scenario/scenario_2470_miss_reversal_reduce.cpp（2470 号段）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kJ152FlagBase = 990177000;   // 177 的 MISS 旗（按自身）

// 177 的 ROUND_END tick：payload = [self, denom] → 有旗则回血并清旗。
EffectResult j152_177_round_end_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int denom = args.int_args[1];
    if (self < 0 || self > 1 || denom <= 0) {
        return EffectResult::kOk;
    }
    const int key = kJ152FlagBase + self;
    const auto it = ctx->plugin_storage.find(key);
    if (it == ctx->plugin_storage.end() || any_int_or(it->second) != 1) {
        return EffectResult::kOk;   // 本回合对手没 MISS
    }
    ctx->plugin_storage.erase(it);
    (void)heal_max_hp_fraction(ctx, self, denom);
    return EffectResult::kOk;
}

// 961 的 ROUND_START tick：payload = [self] → 当回合免攻票。
EffectResult j152_961_round_start_tick(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    if (self < 0 || self > 1) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::DAMAGE, ~0ULL,
                        /*anomaly_mask=*/0, /*duration_rounds=*/1,
                        /*source_id=*/961, /*soul_immunity=*/false,
                        EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/961);
    return EffectResult::kOk;
}

// 522 的减伤回调：任一异常 → final-{amount}（969 同款红伤门）。
void j152_522_reduce(BattleContext* c, int self, int amount, int start, int expire) {
    if (c->roundCount < start || c->roundCount >= expire) {
        return;   // 窗口外
    }
    DamageSnapshot& damage = c->resolvedDamage;
    if (damage.defenderId != self || !damage.isRed || damage.final <= 0) {
        return;   // 只减所受红伤（463 家族口径）
    }
    if (!g_core || !g_core->has_active_anomaly) {
        return;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(c, self, id)) {
            damage.final = std::max(0, damage.final - amount);
            return;
        }
    }
}

}  // namespace

// ── 177 —— {0}回合内若对手MISS则自身恢复1/{1}的最大体力值 ──
// int_args = [owner, target, rounds, denom]（args_num=2）。
EffectResult effect_177_miss_heal(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 177, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 177, args,
                     "177 布局=[owner,target,回合,分母]");
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int rounds = args.int_args[2];
    const int denom = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && opp >= 0 && opp <= 1 && self != opp, 177, args,
                     "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && denom > 0, 177, args, "需要 回合/分母 > 0");
    if (!g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    // MISS 置旗 watcher（160 同款：对手失败事件 + 攻击技过滤）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SKILL_INVALID, self, ctx->roundCount, rounds, /*once=*/false,
        [self, opp](BattleContext* c, const BattleEvent& ev) {
            if (!c || ev.actor != opp) {
                return;
            }
            const int idx = c->roundChoice[opp][1];
            const auto& skills = c->getPet(opp).skills;
            if (idx < 0 || idx >= static_cast<int>(skills.size())
                || skills[static_cast<std::size_t>(idx)].type == SkillType::Attribute) {
                return;   // 属性技不触发（1320 同款过滤）
            }
            c->plugin_storage[kJ152FlagBase + self] = 1;
        },
        WatcherScope::TEAM);
    // ROUND_END 结算 tick（InRounds 窗）。
    register_round_effect(ctx, State::BATTLE_ROUND_END, self, 177, rounds,
                          {self, denom}, &j152_177_round_end_tick);
    return EffectResult::kOk;
}

// ── 961 —— 反转自身能力下降，反转成功则自身下{0}回合免疫攻击伤害 ──
// int_args = [owner, target, rounds]（args_num=1）。
EffectResult effect_961_reversal_immune(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 961, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 3, 961, args,
                     "961 布局=[owner,target,回合]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     961, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0, 961, args, "需要 回合 > 0");
    if (!g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;   // 反转失败（无可反转/被挡）：无免伤窗
    }
    register_next_rounds_effect(ctx, State::BATTLE_ROUND_START, self, 961, rounds,
                                {self}, &j152_961_round_start_tick);
    return EffectResult::kOk;
}

// ── 522 —— {0}回合内若自身处于异常状态则受到伤害减少{1}点 ──
// int_args = [owner, target, rounds, amount]（args_num=2）。
EffectResult effect_522_anomaly_reduce_flat(BattleContext* ctx, const EffectArgs& args) {
    EFFECT_ARG_GUARD(ctx != nullptr, 522, args, "需要 ctx");
    EFFECT_ARG_GUARD(args.int_args != nullptr && args.int_count >= 4, 522, args,
                     "522 布局=[owner,target,回合,点数]");
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int amount = args.int_args[3];
    EFFECT_ARG_GUARD(self >= 0 && self <= 1 && args.int_args[1] >= 0 && args.int_args[1] <= 1,
                     522, args, "需要 owner/target ∈ {0,1}");
    EFFECT_ARG_GUARD(rounds > 0 && amount > 0, 522, args, "需要 回合/点数 > 0");
    const int start = ctx->roundCount;
    const int expire = start + rounds;   // InRounds（含当回合）
    ctx->register_damage_effect(
        DamagePhase::REDUCE_FLAT, self, DamageEffectCategory::MITIGATE,
        [self, amount, start, expire](BattleContext* c, int owner) {
            if (!c || owner != self) {
                return;
            }
            j152_522_reduce(c, self, amount, start, expire);
        });
    return EffectResult::kOk;
}

SKILL_EFFECT(177, effect_177_miss_heal)
SKILL_EFFECT(961, effect_961_reversal_immune)
SKILL_EFFECT(522, effect_522_anomaly_reduce_flat)
