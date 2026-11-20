#include <utils/sbs_rand.h>
// 3414 混沌魔君索伦森的专属技能组（配套魂印 1011，见 soul_lib/chaos_sorensen.cpp）。
//
// （2026-09-18 新建。新文件而不塞进既有家族，是因为这一批 9 个 effect 全部只被 3414 一系用到，
//   且都带"自身处于能力提升状态"这个共同判据——与 3414 的魂印 ③④ 同源，
//   放一起便于将来做 3312 魔君索伦森时整体复用。）
//
// 技能清单（moves 表 3414 可学 25 招，其中 8 招的 effect 全部已在别的家族实现，本文件只补缺口）：
//   30994 烈火净世击（第五技能）150 威力 95 命中：[1021] 条件增伤 + [1216] 反强化并回满
//   30993 混沌主宰 140 威力 先制+1：[1131] 随机固伤（有强化时提档）
//   30992 诸雄之主 85 威力 先制+3：[928] 消回合成功→免下2次异常 + [1129] 概率3倍伤害
//   30472 万虚魔劫破 150 威力：[926] 反自身下降→下2回合先制+2 + [1021] 条件增伤
//   30471 意念魔形 140 威力 先制+1：[1131]
//   30470 魔灵千星 85 威力 先制+3：[928] + [1129]（20% 版）
//   25604 黯灭君王 [191/1047/1215]、25605 背弃圣灵 [585/1005/693/843]、
//   25341 魔君圣威 [585/43/693/843]、25340 混沌魔域 [191/1047/1130] —— 只有 1047/1215/1130 是缺口
//
// ★ "自身处于能力提升状态"的统一判据（1021 的反面 / 1129 / 1131 / 1216 都用它）：
//   5 项战斗属性（攻击/特攻/防御/特防/速度）**任一为正等级**。与魂印侧
//   chaos_sorensen.cpp 的 self_has_stat_boost 同口径，与 skills 侧 962/911 一族同源。
//   ⚠️ 读 **本体** `ctx->ability_levels`，不读 `ws.view_levels`——"视强为弱"类效果改的是
//      视图，官方"处于能力提升状态"判的是**真实等级**（用户 2026-09-13 口径）。
#include "common.h"

namespace {

constexpr int kCombatStatCount = 6;   // 全属性 = 六项能力提升：攻击/特攻/防御/特防/速度/**命中**
//（用户 2026-09-18 口径：「但凡提到等级提升下降的，都是指包含命中的 6 种」；
//  原先写 5 是把槽 5 误当成'体力'，而槽 5 实为命中——体力没有等级）。

// "全属性"判据：本方 5 项战斗属性任一为正等级。
bool side_has_stat_boost(BattleContext* ctx, int side) {
    for (int i = 0; i < kCombatStatCount; ++i) {
        if (ctx->ability_levels[side][i] > 0) {
            return true;
        }
    }
    return false;
}

// 全属性弱化：逐项走 stat_drop（各自查附加禁令 ATTACH_BAN（actor）、免弱 STAT_DROP、
// 各自钳 -6，可部分生效）。与 749「{0}%令对手全属性-{1}」同一落地方式。
void all_stats_down(BattleContext* ctx, int target, int delta, int actor) {
    if (!ctx || !g_core || !g_core->stat_drop || delta <= 0) {
        return;
    }
    for (int i = 0; i < kCombatStatCount; ++i) {
        g_core->stat_drop(ctx, target, i, delta, actor);
    }
}

// 异常是否**真的落地**（apply_anomaly 的四种"成功"变体）。
// 用于"未触发则…"分支：免疫/被挡/反弹/抗性抵抗都算未触发（同 936 的口径，但把
// 286 的"同种异常延长/覆盖"也算落地——那两种官方确实是中了）。
bool anomaly_landed(ApplyAnomalyResult r) {
    return r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED
        || r == ApplyAnomalyResult::CONVERTED;
}

// ── Effect 1021 —— 对手**不处于能力提升状态**时造成的伤害提高{0}%（烈火净世击/万虚魔劫破）──
// Args: [self, target, pct]。
// 落 `ws.damage_add_pct`（**通用增伤·加算**通道，AMP 阶段读）：官方 L352 的判据是措辞里的
// "**额外**"——本模板没有"额外"，故走加算通道（与 776「下N回合伤害翻倍」= 加算 +100 同族）。
// 条件在 SKILL_EFFECT 时点求值：等级在 SKILL_EFFECT→ATTACK_DAMAGE 之间不会再变
//（伤害本身不改等级；暴击破防只清防御正等级，而那正发生在本技能自己的结算尾），
// 故与"伤害结算时判定"等价。
// ⚠️ 写在 ws 是**本回合**有效（ws 每回合 reset）：本技能打空（miss/被盔）时增幅会留在
//    ws 里直到回合末——理论上会影响同回合的第二次出手（额外行动）。当前无此组合，
//    若将来撞上，改成"按本次攻击的管线条目 + 一次性凭证"。
EffectResult effect_1021_boosted_damage_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int pct = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || pct == 0) {
        return EffectResult::kOk;
    }
    if (side_has_stat_boost(ctx, target)) {
        return EffectResult::kOk;   // 对手有强化 → 本条不生效
    }
    ctx->ws.damage_add_pct[self] += static_cast<float>(pct);
    return EffectResult::kOk;
}

// ── Effect 1216 —— 反转对手能力提升状态，反转成功则恢复自身所有体力和PP值（烈火净世击）──
// Args: [self, target]（模板 args_num=0，bind_participants 仍会带上 owner/target）。
// 走 stat_boost_reversal 真原语：**弱化类**动作 → 先查免弱(STAT_DROP)，免疫则 BLOCKED 不恢复；
// 可穿强化保护(STAT_CLEAR)。"反转成功" = REVERSED（对手本无提升则 NOTHING，不恢复）。
EffectResult effect_1216_reverse_boosts_full_restore(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core
        || !g_core->stat_boost_reversal || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal(ctx, target) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    ElfPet& pet = ctx->getPet(self);
    if (pet.hp <= 0) {
        return EffectResult::kOk;
    }
    // 恢复**所有**体力：heal_amount 内部钳满（吃封回血与恢复效果修正）。
    g_core->heal_amount(ctx, self, max_hp_of(ctx, self));
    // 恢复**所有** PP：pp == -1 = 无限 PP 的技能跳过（同 effect 87 口径）。
    for (Skills& sk : pet.skills) {
        if (sk.pp >= 0) {
            sk.pp = sk.maxPP;
        }
    }
    return EffectResult::kOk;
}

// ── Effect 1131 —— 随机附加{0}-{1}点固定伤害，自身处于能力提升状态时变为{2}-{3}点（混沌主宰/意念魔形）──
// Args: [self, target, lo, hi, lo2, hi2]。
// 固伤走 deal_pink_damage（FIXED 档，吃护罩/固定抗性/免粉）；区间为**闭区间均匀掷点**
//（同 effect 683「随机附加 low~high 点固定伤害」— 掷点在动作分支内消耗 rand，本效果无概率门）。
EffectResult effect_1131_random_fixed_boosted(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 6 || !g_core || !g_core->deal_pink_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const bool boosted = side_has_stat_boost(ctx, self);
    // 有强化用提档区间 [lo2, hi2]，否则用 [lo, hi]（模板与数据库 args 同序）。
    const int lo = boosted ? args.int_args[4] : args.int_args[2];
    const int hi = boosted ? args.int_args[5] : args.int_args[3];
    if (lo < 0 || hi < lo) {
        return EffectResult::kOk;
    }
    const int amount = lo + (sbs_rand() % (hi - lo + 1));
    if (amount > 0) {
        g_core->deal_pink_damage(ctx, target, amount, DamageKind::FIXED, self);
    }
    return EffectResult::kOk;
}

// ── Effect 1129 —— {0}%概率伤害为{1}倍，自身处于能力提升状态时概率翻倍（诸雄之主/魔灵千星）──
// Args: [self, target, chance, mult]。
// "伤害为{1}倍" 落在**通用增伤·加算**通道（`ws.damage_add_pct`）：与 776「伤害翻倍」同一族
// （官方 L352 判据是"额外"二字，本模板没有）。
// ⚠️ "概率翻倍"= **概率值**翻倍（20%→40%），不是伤害倍数翻倍（用户 2026-09-13 同族口径）。
EffectResult effect_1129_chance_damage_multiplier(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int chance_arg = args.int_args[2];
    const int mult = args.int_args[3];
    if (self < 0 || self > 1 || chance_arg <= 0 || mult <= 1) {
        return EffectResult::kOk;
    }
    int chance = chance_arg;
    if (side_has_stat_boost(ctx, self)) {
        chance *= 2;
    }
    if (chance > 100) {
        chance = 100;
    }
    if (!roll_percent(chance)) {
        return EffectResult::kOk;
    }
    // mult 倍 = 加算 (mult-1)*100 个百分点（×3 → +200）。
    ctx->ws.damage_add_pct[self] += static_cast<float>((mult - 1) * 100);
    return EffectResult::kOk;
}

// ── Effect 926 —— 反转自身能力下降状态，反转成功则下{0}回合先制+{1}（万虚魔劫破）──
// Args: [self, target, rounds, bonus]。
// 走 stat_reversal 真原语（**反转**下降→提升，区别于 clear_stat_boosts 的"消除提升"）。
// 反转成功（REVERSED，即本有下降）才给先制窗口。
// 窗口起点按官方"回合数判定"取 **NextRounds**（"下{0}回合"= 本回合不算，R+1..R+N）：
// 与 843「下{0}回合令自身所有技能先制+{1}」同族同口径——843 在认证数据层
// （custom_effect_overrides.window=next_rounds）已声明，926 尚未登记，故直接走
// register_next_rounds_effect（该表是跨 agent 共享的只读数据件，插件不改；将来补声明后
// 可换成 register_round_effect 走声明查询）。
EffectResult effect_926_reverse_drops_next_priority(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 4 || !g_core || !g_core->stat_reversal) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int rounds = args.int_args[2];
    const int bonus = args.int_args[3];
    if (self < 0 || self > 1 || rounds <= 0 || bonus == 0) {
        return EffectResult::kOk;
    }
    if (g_core->stat_reversal(ctx, self) != StatReversalResult::REVERSED) {
        return EffectResult::kOk;
    }
    register_next_rounds_effect(ctx, State::BATTLE_FIRST_MOVE_RIGHT, self, /*effect_id=*/926,
                                rounds, std::vector<int>{self, 1 - self, bonus},
                                &effect_move_right_priority_bonus);
    return EffectResult::kOk;
}

// ── Effect 928 —— 消除对手回合类效果，消除成功则己方免疫下{0}次受到的异常状态（诸雄之主/魔灵千星）──
// Args: [self, target, counts]。三态语义直接用断回合原语返回值区分：
// SUCCESS → 给次数型免异常；NO_EFFECTS（本来就没有）/ IMMUNE（对手免断）→ 一概不给。
// 免异常走 RuleCenter 的 ANOMALY 票（counts=N，每被施加一次扣一次）——全时点覆盖，
// 与 1221「反转成功则自身免疫下{n}次异常」同一落地方式。
EffectResult effect_928_break_then_anomaly_immunity(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->break_round_effects) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int counts = args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (self < 0 || self > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->break_round_effects(ctx, target) != BreakResult::SUCCESS) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(self, ImmunityType::ANOMALY, coverage_all(), /*anomaly_mask=*/0,
                        /*duration_rounds=*/0, /*source_id=*/928,
                        /*soul_immunity=*/false, EffectScope::ON_STAGE,
                        /*counts=*/counts, /*source_effect_id=*/928);
    return EffectResult::kOk;
}

// ── Effect 1047 —— {0}%令对手{1}，**未触发**则吸取对手最大体力的1/{2}（黯灭君王/混沌魔域）──
// Args: [self, target, chance, anomaly_id, denom]。
// 概率门掷**一次**：命中 → 现代通道施加异常（免疫/抗性/反弹/转化全在 core 侧按序处理，
// 这里只认"最终有没有中"）；未落地（含被免疫/被挡/被弹回）→ 吸取。同 936 的双分支结构。
EffectResult effect_1047_anomaly_or_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 5 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int chance = args.int_args[2];
    const int anomaly = args.int_args[3];
    const int denom = args.int_args[4];
    if (self < 0 || self > 1 || target < 0 || target > 1
        || !is_valid_abnormal_status_id(anomaly)) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        const ApplyAnomalyResult r =
            g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), self);
        if (anomaly_landed(r)) {
            return EffectResult::kOk;   // 中了 → 不吸取
        }
    }
    if (denom > 0 && g_core->drain_hp) {
        g_core->drain_hp(ctx, self, target, denom);   // 掉 max_hp/denom 固伤 + 自身回等量
    }
    return EffectResult::kOk;
}

// ── Effect 1215 —— 消除对手所有护盾效果，消除成功则使对手全属性-{0}，
//                  自身体力低于对手时弱化效果翻倍（黯灭君王）──
// Args: [self, target, delta]。
// "消除成功" = 对手**本来有护盾**（护盾值 > 0）才算成功，才给弱化。
// ⚠️ 只清**护盾**（`shield_bank_`），不动**护罩**（`hood_bank_`）——官方把两者分列
//   （"消除所有护盾和护罩"才是两清），本模板只写"护盾效果"。
// 清完 emit EVENT_SHIELD_BROKEN（与管线击破护盾同一事件；"护盾被击破时…"类监听器照常收到）。
EffectResult effect_1215_clear_shield_then_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || delta <= 0) {
        return EffectResult::kOk;
    }
    ShieldBank& shields = ctx->getPet(target).shield_bank_;
    const int removed = shields.total();
    if (removed <= 0) {
        return EffectResult::kOk;   // 没有护盾可消 → 消盾未成功，不给弱化
    }
    shields.clear();
    ctx->event_center_.emit(BattleEvent{EventType::EVENT_SHIELD_BROKEN, self, target, removed});
    // "自身体力低于对手时弱化效果翻倍"：比较**当前**体力（同 946/885 的体力比较口径）。
    const int mult = (ctx->getPet(self).hp < ctx->getPet(target).hp) ? 2 : 1;
    all_stats_down(ctx, target, delta * mult, args.int_args[0]);
    return EffectResult::kOk;
}

// ── Effect 1130 —— 使对手全属性-{0}，自身体力低于对手时弱化效果翻倍（混沌魔域）──
// Args: [self, target, delta]。与 946「全属性+N，自身体力低于对手时翻倍」互为镜像（方向相反）。
EffectResult effect_1130_all_stats_down_hp_lower(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int target = args.int_args[1];
    const int delta = args.int_args[2];
    if (self < 0 || self > 1 || target < 0 || target > 1 || delta <= 0) {
        return EffectResult::kOk;
    }
    const int mult = (ctx->getPet(self).hp < ctx->getPet(target).hp) ? 2 : 1;
    all_stats_down(ctx, target, delta * mult, args.int_args[0]);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
    SKILL_EFFECT(1021, effect_1021_boosted_damage_amp);            // 对手无提升→伤害+{0}%
    SKILL_EFFECT(1216, effect_1216_reverse_boosts_full_restore);   // 反对手提升成功→回满血+PP
    SKILL_EFFECT(1131, effect_1131_random_fixed_boosted);          // 随机固伤（有强化提档）
    SKILL_EFFECT(1129, effect_1129_chance_damage_multiplier);      // {0}%伤害{1}倍（有强化概率翻倍）
    SKILL_EFFECT(926, effect_926_reverse_drops_next_priority);     // 反自身下降成功→下N回合先制+N
    SKILL_EFFECT(928, effect_928_break_then_anomaly_immunity);     // 消回合成功→免下N次异常
    SKILL_EFFECT(1047, effect_1047_anomaly_or_drain);              // {0}%异常，未触发则吸取1/{2}
    SKILL_EFFECT(1215, effect_1215_clear_shield_then_drop);        // 消盾成功→对手全属性-{0}
    SKILL_EFFECT(1130, effect_1130_all_stats_down_hp_lower);       // 对手全属性-{0}（体力低翻倍）

}  // namespace
