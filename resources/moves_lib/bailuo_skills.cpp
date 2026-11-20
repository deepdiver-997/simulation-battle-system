// 百罗鬼帝 4444 技能组（DB 可学表 monster_id=4444；4399 图鉴 1012193 对照）
//
// 本文件落的是**全局缺号**的专属效果（已存在的直接复用，注册表只增不改）：
//   · 38383 蔑天行·残虐（第五技，威力150 PP4 必中）：
//       881  条件先制+3 —— 已有（burn_trio_skills.cpp，先手权判定时点）
//       2470 解除自身能力下降；自身无下降则改为消除对手能力提升
//       2471 {0}%令对手臣服(31)，未触发则消除对手回合类效果并令自身狂暴(14)
//       2377 对手每处于1种能力下降威力提升{0}点（变威力）
//       2440 未击败对手时恢复当回合所造成攻击伤害的{0}%体力；未造成攻击伤害则
//            对手{1}回合内无法主动切换精灵（SWITCH_LOCK 锁切票）
//   · 38382 尸走魂嚎：
//       1297 消除回合类→成功令对手{0}(23 诅咒)/未触发吸血1/{1} —— 已有（tianqi_dijun）
//       1110 反转对手能力提升；成功则对手下{0}次属性技能无效；失败则消除对手能力提升
//       2473 己方每阵亡1只精灵，每次在场限1次，获得1种自身没有的鬼祰
//            （鬼祰权威状态在 plugin_storage int 槽 —— resources/bailuo_runtime.h，
//            与魂印 2085 的登场分支读写同一份）
//
// ⚠️ **参数缺口（DB 侧）**：2470/2471/2377/2440 是新代效果，`side_effect` 表没有
//    它们的 arg_count 行 → build_skill_effects 的顺序切片分不到参数（args 全空）。
//    本文件对缺参处取 4399 图鉴文本的定值兜底（100%/31/14、30、100%/1），并在
//    DB 侧补齐 side_effect.arg_count 后可无缝切换到读参。
// ⚠️ 2440 的"未击败/未造成攻击伤害"要在伤害落地后判 → effect_register_state 加了
//    AFTER_ACTION 分支（1533/1218 同款归位）。
#include "common.h"

#include "../bailuo_runtime.h"

namespace {

// ── 2470 解除自身能力下降；自身不处于能力下降状态则改为消除对手能力提升 ──
// "解除下降" = clear_stat_drops（措辞即判据：不是 1221/521 的"反转"——那会把负等级
// 翻成正等级，2470 只是把下降洗掉）；返回值 = 洗掉的段数（0 = 自身无下降 → 改消对手强）。
EffectResult effect_2470_self_reversal_else_clear(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_drops && g_core->clear_stat_drops(ctx, owner) > 0) {
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, target);
    }
    return EffectResult::kOk;
}

// ── 2471 {0}%令对手{1}，未触发则消除对手回合类效果并令自身{2} ──
// 官方定值（4399 残虐）：100% / 臣服(31) / 狂暴(14)。臣服 = 弱化类异常 211
// （"无法对对手造成攻击伤害、固定伤害、百分比伤害"），走施加原语（吃免控/抗性）。
EffectResult effect_2471_chance_anomaly_else_break(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    // side_effect.arg_count 缺行 → 切不到参；取官方定值兜底（见文件头）。
    const int chance = args.int_count > 2 && args.int_args[2] > 0 ? args.int_args[2] : 100;
    const int anomaly = args.int_count > 3 && is_valid_abnormal_status_id(args.int_args[3])
        ? args.int_args[3]
        : static_cast<int>(AbnormalStatusId::Submission);
    const int self_fallback = args.int_count > 4 && is_valid_abnormal_status_id(args.int_args[4])
        ? args.int_args[4]
        : static_cast<int>(AbnormalStatusId::Berserk);
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    const ApplyAnomalyResult r =
        g_core->apply_anomaly(ctx, target, anomaly, random_anomaly_duration(), owner);
    const bool landed = r == ApplyAnomalyResult::SUCCESS
        || r == ApplyAnomalyResult::REPLACED_EXISTING
        || r == ApplyAnomalyResult::DURATION_EXTENDED;
    if (landed) {
        return EffectResult::kOk;
    }
    // 未触发 → 消除对手回合类效果 + 令自身狂暴（断回合成功与否都附加狂暴——
    // 官方句式"未触发则 A 并令自身 B"是并列落地，不互斥）
    if (g_core->break_round_effects) {
        g_core->break_round_effects(ctx, target);
    }
    g_core->apply_anomaly(ctx, owner, self_fallback, random_anomaly_duration(), owner);
    return EffectResult::kOk;
}

// ── 2377 对手每处于1种能力下降威力提升{0}点 ──
// 变威力：写 skill_power_view（mortarion 882 镜像——882 数自身提升，2377 数对手下降）。
EffectResult effect_2377_power_per_foe_drop(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    // 缺参兜底：官方定值 30 点/种（4399 残虐文本）
    const int per = args.int_count > 2 && args.int_args[2] > 0 ? args.int_args[2] : 30;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    int drops = 0;
    for (int stat = 0; stat < BattleContext::kAbilityLevelSlotCount; ++stat) {
        if (ctx->ability_level(target, stat) < 0) {
            ++drops;
        }
    }
    if (drops > 0) {
        ctx->ws.skill_power_view[owner] += per * drops;
    }
    return EffectResult::kOk;
}

// ── 2440 未击败对手时恢复当回合所造成攻击伤害的{0}%体力；未造成攻击伤害则
//        对手{1}回合内无法主动切换精灵 ── [AFTER_ACTION]
// "所造成攻击伤害"读裸伤台账（raw_attack_damage，422/1533 同款口径——挡伤/锁伤
// 改变不了它）；"未击败" = 伤害落地后对手 hp>0（1218/1307 同款判据）。
EffectResult effect_2440_heal_or_switch_lock(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    // 缺参兜底：官方定值 100% / 1 回合
    const int heal_pct = args.int_count > 2 && args.int_args[2] > 0 ? args.int_args[2] : 100;
    const int lock_rounds = args.int_count > 3 && args.int_args[3] > 0 ? args.int_args[3] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(target).hp <= 0) {
        return EffectResult::kOk;   // 已击败对手：两个分支都不走
    }
    const int dealt = ctx->ws.raw_attack_damage[owner];
    if (dealt > 0) {
        if (g_core->heal_amount) {
            g_core->heal_amount(ctx, owner, dealt * heal_pct / 100);
        }
        return EffectResult::kOk;
    }
    // 未造成攻击伤害 → 锁切（挂对手身上，绑定方语义 2026-09-21 口径：被锁方切换即清）
    ctx->rule_center_.grant_switch_lock(owner, /*source_effect_id=*/2440, target,
                                        lock_rounds, ctx->roundCount, EffectScope::ON_STAGE,
                                        ctx->round_effect_valid_id[target]);
    return EffectResult::kOk;
}

// ── 1110 反转对手能力提升状态，反转成功则对手下{0}次属性技能失效，反转失败则
//        消除对手能力提升状态 ──
// 反转走 stat_boost_reversal（143/743 同款，第一件事查免弱）；封属 = SEAL_ATTRIBUTE
// 按次（counts=N rounds=0）；"消除" = clear_stat_boosts（查 STAT_CLEAR）。
EffectResult effect_1110_reversal_then_seal(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2 || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    const int target = args.int_args[1];
    const int seal_counts = args.int_count > 2 && args.int_args[2] > 0 ? args.int_args[2] : 1;
    if (owner < 0 || owner > 1 || target < 0 || target > 1) {
        return EffectResult::kOk;
    }
    if (g_core->stat_boost_reversal
        && g_core->stat_boost_reversal(ctx, target) == StatReversalResult::REVERSED) {
        ctx->grant_skill_invalid(owner, target, /*source_slot=*/-1, /*effect_id=*/1110,
                                 SealKind::SEAL_ATTRIBUTE, /*counts=*/seal_counts,
                                 /*rounds=*/0, /*penetrable=*/false, EffectScope::ON_STAGE);
        return EffectResult::kOk;
    }
    if (g_core->clear_stat_boosts) {
        g_core->clear_stat_boosts(ctx, target);
    }
    return EffectResult::kOk;
}

// ── 2473 己方每阵亡1只精灵，每次在场限1次，获得1种自身没有的鬼祰 ──
// "每次在场限1次" = on_stage_storage 戳（换宠自动清 = 本次上场生命周期）；
// "己方阵亡" = count_dead(OWN ALL)（帝君之阵同口径：背包+额外，消逝不计）。
EffectResult effect_2473_guixiao_on_ally_death(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core || !g_core->count_dead) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    constexpr int kOnceStampKey = 2473;
    if (self.on_stage_storage.count(kOnceStampKey)) {
        return EffectResult::kOk;   // 本次在场已触发过
    }
    if (g_core->count_dead(ctx, owner, DeadScope::ALL) <= 0) {
        return EffectResult::kOk;   // 己方没有阵亡精灵
    }
    self.on_stage_storage[kOnceStampKey] = 1;
    bailuo::acquire_kind_not_held(ctx, owner);
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册；注册总数只增不改）──
SKILL_EFFECT(2470, effect_2470_self_reversal_else_clear);
SKILL_EFFECT(2471, effect_2471_chance_anomaly_else_break);
SKILL_EFFECT(2377, effect_2377_power_per_foe_drop);
SKILL_EFFECT(1110, effect_1110_reversal_then_seal);
SKILL_EFFECT(2473, effect_2473_guixiao_on_ally_death);

}  // namespace
