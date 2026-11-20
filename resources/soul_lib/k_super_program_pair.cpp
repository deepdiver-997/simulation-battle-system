// K14 超级形态重程序对 —— effect 2441/2442（2 宏 4 行；行≠宏：基础+超级同 id，
// 按 monster_id 分流——4825/4829 卡布达、4826/4830 鲨鱼辣椒）
//
// 行清单（tools/soul_effects_dump.py 2441 2442，2026-10-05 全文）：
//   4825 卡布达 [0,40,0,0,1]：
//     ①ROUND_START 对手无提升→当回合攻伤+40（db[1]）；
//     ②ROUND_START 对手无回合类效果→当回合全技先制+1；
//     ③用技能后吸取对手最大体力 20%（drain_hp denom=5）
//   4829 卡布达超级 [30,50,3,10,1]：4825 三条（40→50）+「同时满足→回合末回 1/3」
//     +登场按每 30% 体分段免疫麻痹；尾段记档（见下）
//   4826 鲨鱼辣椒 [0,25,0,0,1]：
//     ①对手无流血→受到的红/固/百三类伤害各-25%，每回合挨满 2 类后剩余类翻倍；
//     ②对手有流血→自身免疫控制类异常
//   4830 鲨鱼辣椒超级 [30,25,2,100,1]：4826 两条 + 登场按对手每 30% 体挂流血
//     （已流血则流血回合数+段数 + 自身 1 回合免降）+ 回合末吸取 100+流血余回合×100
//
// ⚠️ 记档（本单不做，挂账重程序尾段）：
//   - 4829 尾段「异常每被免疫/转化 1 次→吸取效果+10% cap60%、满后全异常免疫」——
//     需要"免疫成功消费"事件钩子，内核无现成监听面；
//   - 4829/4825 ②的「免疫成功则消除对手回合类与提升」同因（免疫成功检测面缺）；
//   - 4830 尾段「对手免疫固定伤害则消除回合类」与「每回合前 2 次触发后流血+1」——
//     触发计数语义（哪 2 次）需官方实测裁决。
//
// 落点：先制+1 必须挂 FIRST_MOVE_RIGHT 桶（J115 勘误：ROUND_START 写会被
// handle_BattleFirstMoveRight 入口 memset 冲掉）；吸取走 g_core->drain_hp（恢复走
// heal_impl、封回血生效）；流血 Bleed=16 + set_anomaly_rounds；控制类免疫 = 迭代
// abnormal_status_kind==Control 拼 anomaly_mask（grant_immunity ANOMALY 按位）；
// 三类伤害减 = 红伤 REDUCE_PCT 票 + 粉伤 REDUCE_EXTRA 票（回调现查流血门），类计数
// plugin_storage[round 基键+类] 打标，满 2 类后第三类减半翻倍。
// 三查记录（2026-10-05）：①内核零硬编码；②effect_info 技能空间模板无关；
// ③registry 无四 id，未合并分支无实现。
#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

constexpr int kBleedId = static_cast<int>(AbnormalStatusId::Bleed);
constexpr int kKindRed = 0;   // 回合内类计数键：+0 红 /+1 固 /+2 百
constexpr int kKindFixed = 1;
constexpr int kKindPercent = 2;

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
};

HostRow host_row(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = resolve_subject_slot_from_args(args);
    if (slot < 0) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    const ElfPet& host = ctx->seerRobot[owner].elfPets[slot];
    row.db = &host.soulMark.args.owned_int_args;
    row.monster_id = host.soulMark.monster_id;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

bool armed_this_epoch(BattleContext* ctx, int owner, int storage_key) {
    ElfPet& pet = ctx->getPet(owner);
    auto it = pet.soulmark_storage.find(storage_key);
    if (it == pet.soulmark_storage.end()) {
        it = pet.soulmark_storage.emplace(storage_key, int{-1}).first;
    }
    int* armed_epoch = std::any_cast<int>(&it->second);
    if (!armed_epoch) {
        return true;
    }
    if (*armed_epoch == ctx->pipeline_valid_id[owner]) {
        return true;
    }
    *armed_epoch = ctx->pipeline_valid_id[owner];
    return false;
}

bool armed_once(BattleContext* ctx, int owner, int key) {
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

bool opponent_has_any_boost(BattleContext* ctx, int owner) {
    const int opp = ctx->opponent(owner);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[opp][i] > 0) {
            return true;
        }
    }
    return false;
}

bool opponent_bleeding(BattleContext* ctx, int owner) {
    return g_core && g_core->has_active_anomaly
        && g_core->has_active_anomaly(ctx, ctx->opponent(owner), kBleedId);
}

// ⚠️ 必须 64 位：控制类异常 id 上限 43 > 31，32 位 int 的 (1 << id) 是移位 UB
//   （UBSan 抓现行：night fuzz 2026-10-06，全部 seed 首战即 abort）；非消毒环境
//   在 x86 上是移位计数取模 32 的静默位错乱。下游 RuleTicket::anomaly_mask
//   本就是 uint64_t（rule_center.h）。
uint64_t control_anomaly_mask() {
    uint64_t mask = 0;
    for (int id = 0; id <= 43; ++id) {
        if (abnormal_status_kind(static_cast<AbnormalStatusId>(id))
            == AbnormalStatusKind::Control) {
            mask |= (1ULL << id);
        }
    }
    return mask;
}

// ── 2441/2442 共享 ROUND_START（攻伤窗 + 双条件 heal 旗标）───────────────────

EffectResult super_round_start(BattleContext* ctx, const EffectArgs& args, int mark_id,
                               int amp_pct) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, mark_id)) {
        return EffectResult::kOk;
    }
    const bool no_boost = !opponent_has_any_boost(ctx, owner);
    const bool no_round = !ctx->has_round_effects(ctx->opponent(owner));
    if (no_boost && amp_pct > 0) {
        ctx->ws.damage_add_pct[owner] += amp_pct;   // 当回合攻伤窗
    }
    // 双条件旗标（4829「同时满足→回合末回 1/3」；4825 无此子句，旗标无人消费）
    ctx->plugin_storage[900000000 + mark_id * 10 + owner] = int{no_boost && no_round ? 1 : 0};
    return EffectResult::kOk;
}

EffectResult soulmark_2441_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 2441);
    // amp 走行参（基础行 db[1]=40 / 超级行 db[1]=50）
    return super_round_start(ctx, args, 2441, db_at(row, 1, 40));
}

// 先制+1：FIRST_MOVE_RIGHT 桶（J115 勘误正位）；条件=对手无回合类效果。
EffectResult super_preempt(BattleContext* ctx, const EffectArgs& args, int mark_id) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, mark_id)) {
        return EffectResult::kOk;
    }
    if (!ctx->has_round_effects(ctx->opponent(owner))) {
        ctx->ws.preemptive_level[owner] += 1;
    }
    return EffectResult::kOk;
}

EffectResult soulmark_2441_preempt(BattleContext* ctx, const EffectArgs& args) {
    return super_preempt(ctx, args, 2441);
}

// 双满足 → 回合末回 1/3（仅超级行 monster 4829 消费旗标；基础行键恒 0）
EffectResult soulmark_2441_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 2441)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 2441);
    if (row.monster_id != 4829) {
        return EffectResult::kOk;   // 双满足回血是超级行子句
    }
    const int key = 900000000 + 2441 * 10 + owner;
    const int flag = any_int_or(ctx->plugin_storage[key], 0);
    if (flag == 1 && g_core && g_core->heal_amount) {
        const int max_hp = resolve_pet_max_hp(ctx->getPet(owner));
        g_core->heal_amount(ctx, owner, max_hp / 3);
    }
    ctx->plugin_storage[key] = int{0};
    return EffectResult::kOk;
}

// ③用技能后/用攻击技能后吸取 20%（4825 全技 / 4829 攻击技限定）
EffectResult super_drain(BattleContext* ctx, const EffectArgs& args, int mark_id,
                         bool attack_only) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, mark_id)) {
        return EffectResult::kOk;
    }
    if (ctx->roundChoice[owner][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    if (attack_only) {
        const int idx = ctx->roundChoice[owner][1];
        if (idx < 0 || idx >= 5 || ctx->getPet(owner).skills[idx].type == SkillType::Attribute) {
            return EffectResult::kOk;
        }
    }
    if (g_core && g_core->drain_hp) {
        g_core->drain_hp(ctx, owner, ctx->opponent(owner), /*fraction_denom=*/5);
    }
    return EffectResult::kOk;
}

EffectResult soulmark_2441_drain(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 2441);
    return super_drain(ctx, args, 2441, /*attack_only=*/row.monster_id == 4829);
}

// 超级行（monster 4829）登场：每 30% 体 → 1 回合麻痹免疫（段数=hp*10/max）
EffectResult soulmark_2441_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 2441)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 2441);
    if (row.monster_id != 4829) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 2441 * 100 + 1)) {
        return EffectResult::kOk;
    }
    const ElfPet& self = ctx->getPet(owner);
    const int max_hp = resolve_pet_max_hp(self);
    if (max_hp <= 0) {
        return EffectResult::kOk;
    }
    const int segments = self.hp * 10 / (max_hp * 3);   // 每 30% 一段
    if (segments <= 0) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        /*anomaly_mask=*/1 << 0 /*麻痹*/, segments,
                        2441 * 10, true, EffectScope::ON_STAGE, 0, 2441);
    return EffectResult::kOk;
}

// ── 2442 家族：三类伤害减 + 流血门控制免疫 ─────────────────────────────────

// 类计数：本回合该类是否已挨过（1=挨过）。翻倍条件=已挨满 2 类后本类第一次挨。
bool kind_already_hit(BattleContext* ctx, int owner, int kind) {
    return any_int_or(ctx->plugin_storage[800000000 + ctx->roundCount * 8 + owner * 4 + kind],
                      0) == 1;
}

void mark_kind_hit(BattleContext* ctx, int owner, int kind) {
    ctx->plugin_storage[800000000 + ctx->roundCount * 8 + owner * 4 + kind] = int{1};
}

int kinds_hit_count(BattleContext* ctx, int owner) {
    int n = 0;
    for (int k = 0; k < 3; ++k) {
        n += kind_already_hit(ctx, owner, k) ? 1 : 0;
    }
    return n;
}

EffectResult soulmark_2442_arm(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 2442 * 100 + 9)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 2442);
    const int pct = db_at(row, 1, 25);
    if (pct <= 0) {
        return EffectResult::kOk;
    }
    // 红伤 -25%（回调现查流血门 + 类计数翻倍）
    ctx->register_damage_effect(
        DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
        [owner, pct](BattleContext* c, int bucket_owner) {
            DamageSnapshot& d = c->resolvedDamage;
            if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 2442)
                || opponent_bleeding(c, bucket_owner)) {
                return;
            }
            const int mult = kinds_hit_count(c, bucket_owner) >= 2 && !kind_already_hit(c, bucket_owner, kKindRed) ? 2 : 1;
            d.final = d.final * (100 - pct * mult) / 100;
            mark_kind_hit(c, bucket_owner, kKindRed);
        });
    // 粉伤（固定/百分比两类）：REDUCE_EXTRA 百分比免减
    ctx->register_pink_damage_effect(
        PinkDamagePhase::REDUCE_EXTRA, owner,
        [owner, pct](BattleContext* c, int bucket_owner) {
            PinkDamageResolved& p = c->resolvedPink;
            if (p.target != bucket_owner || p.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, bucket_owner, 2442)
                || opponent_bleeding(c, bucket_owner)) {
                return;
            }
            const int kind =
                p.tier == PinkDamageTier::FIXED ? kKindFixed : kKindPercent;
            const int mult = kinds_hit_count(c, bucket_owner) >= 2 && !kind_already_hit(c, bucket_owner, kind) ? 2 : 1;
            p.final = p.final * (100 - pct * mult) / 100;
            mark_kind_hit(c, bucket_owner, kind);
        });
    return EffectResult::kOk;
}

// 对手有流血 → 免疫控制类异常（每回合 ROUND_START 重挂，duration 1）
EffectResult soulmark_2442_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 2442)) {
        return EffectResult::kOk;
    }
    if (!opponent_bleeding(ctx, owner)) {
        return EffectResult::kOk;
    }
    ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                        control_anomaly_mask(), 1, 2442 * 10, true,
                        EffectScope::ON_STAGE, 0, 2442);
    return EffectResult::kOk;
}

// 4830 登场：对手每 30% 体挂流血 1 回合（段数=duration）；已流血→回合数+段数+自身免降
EffectResult soulmark_4830_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 2442)) {
        return EffectResult::kOk;
    }
    if (armed_this_epoch(ctx, owner, 2442 * 100 + 3)) {
        return EffectResult::kOk;
    }
    const int foe = ctx->opponent(owner);
    const ElfPet& opp = ctx->getPet(foe);
    const int opp_max = resolve_pet_max_hp(opp);
    if (opp_max <= 0 || !g_core || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    const int segments = opp.hp * 10 / (opp_max * 3);   // 每 30% 一段
    if (segments <= 0) {
        return EffectResult::kOk;
    }
    if (opponent_bleeding(ctx, owner)) {
        if (g_core->set_anomaly_rounds) {
            const int cur_end = g_core->get_abnormal_status_end_round
                                    ? g_core->get_abnormal_status_end_round(ctx, foe, kBleedId)
                                    : 0;
            const int cur_left = cur_end > ctx->roundCount ? cur_end - ctx->roundCount : 0;
            g_core->set_anomaly_rounds(ctx, foe, kBleedId, cur_left + segments);
        }
        // 自身 1 回合免降
        ctx->grant_immunity(owner, ImmunityType::STAT_DROP, coverage_all(), 0, 1,
                            2442 * 10 + 1, true, EffectScope::ON_STAGE, 0, 2442);
    } else {
        g_core->apply_anomaly(ctx, foe, kBleedId, segments, owner);
    }
    return EffectResult::kOk;
}

// 4830 回合末：吸取 100 + 对手流血余回合×100
EffectResult soulmark_4830_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 2442)) {
        return EffectResult::kOk;
    }
    const int foe = ctx->opponent(owner);
    int amount = 100;
    if (g_core && g_core->get_abnormal_status_end_round) {
        const int cur_end = g_core->get_abnormal_status_end_round(ctx, foe, kBleedId);
        if (cur_end > ctx->roundCount) {
            amount += (cur_end - ctx->roundCount) * 100;
        }
    }
    if (g_core && g_core->deal_pink_damage && g_core->heal_amount) {
        const ElfPet& opp = ctx->getPet(foe);
        const int deal = std::min(amount, std::max(opp.hp, 0));
        if (deal > 0) {
            g_core->deal_pink_damage(ctx, foe, deal, DamageKind::FIXED, owner);
            g_core->heal_amount(ctx, owner, deal);
        }
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +2（2441/2442；只增不改）──────
SOUL_PROGRAM(2441, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2441_enter,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2441_round_start),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &soulmark_2441_preempt),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_2441_round_end),
                   SoulMarkNodeRef(State::BATTLE_FIRST_SKILL_EFFECT, &soulmark_2441_drain),
                   SoulMarkNodeRef(State::BATTLE_SECOND_SKILL_EFFECT, &soulmark_2441_drain)});
SOUL_PROGRAM(2442, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_2442_arm,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4830_enter,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_2442_round_start),
                   SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_4830_round_end)});
