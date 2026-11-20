// K389 满体免疫+未满回血等量固伤族 —— effect 1522（1 宏 / 2 行）
//（极寒魔兽 3926 row1191 args[3,50,1] 本体 + row1268 args[3,70,1] 伙伴行——吸血率
//  50%/70% 差异；号段 7420-7429，worktree ../agents/k389，2026-10-24）
//
// 行清单（tools/soul_effects_dump.py 1522，2026-10-24）：
//   两行同文：「回合开始时，若自身满体力则当回合免疫异常状态，若自身未满体力则战斗
//   阶段结束时恢复自身最大体力的1/3并造成等量固伤；自身攻击造成伤害的{50|70}%恢复
//   自身体力，若回合开始时对手处于异常状态则附加等量固定伤害（boss无效）」
//
// ⚠️ 行≠模板：effect_info 1522 异文（固伤翻倍模板）——tips 权威（板行切片核查已核）。
//
// 行参解码（两行同构互锚）：
//   db[0]=①恢复分母（3→1/3）、db[1]=②吸血%（50/70）、db[2]=旗恒 1（语义不可辨——
//   按 K1 纪律不消费，记档）。
//
// 落法（板行处方逐子句）：
//   ① 回合开始满体力门（条件时点="回合开始时"→ RS 一次采样，非 285
//     "满体力时"逐次判）：满 → grant_immunity(ANOMALY, duration=1)（K171 285 票形，
//     无 condition——采样定生死，回合中途掉血不撤、回血不补）；未满 → 置当回合
//     "未满"门旗（每回合无脑重写，K361 819 形）。
//     ⚠️ ENTER+RS 双节点采样（K384 snap 同位）：R1 的 RS 在 b.start() 停泊点**尚未
//     执行**（2026-10-24 T7420 首版实测红）——enter 首采样让 R1 票赶在部署段生效
//     （K171 "enter 首挂" 同理由）；中途上场以入场时刻为"回合开始"（K384 记档口径）。
//   ①' ROUND_END：未满旗开 → heal_amount(max/db[0])，固伤量 = ws.last_heal_amount
//     （等量=**实际恢复量**——封回血/满血封顶时固伤随之缩，j_opp_attr_reaction 同源
//     读法）→ deal_pink_damage(self, FIXED)。
//   ② enter(early) 接线 DETECT 零状态票（K168 363 直系）：attacker==own && isRed &&
//     final>0 → heal_amount(final×db[1]/100)；ROUND_START 采样的"对手异常"门旗开 →
//     deal_pink_damage(opp, 同量, FIXED)（等量=吸血**名义量** final×db[1]/100，与
//     恢复是否被吃封无关——"攻击造成伤害的50%"是先期计算量；板行处方"对账量=吸血量"）。
//   boss_invalid：tips 尾注（boss无效）→ 全节点统一置位（漏标=半初始化）。
//   行参读取：db_at 读宿主 owned_int_args 原序（方案 B，K257 起夜跑口径）。
//
// 三查记录（2026-10-24，a25-kprod 切片核查 + a82-jprod 动工复核）：
//   ①内核 grep 1522 无魂印行为硬编码；②effect_info 1522 异文、tips 权威（dump 原文
//   已核）；③registry soul 无 1522、soul_lib 全库无 SOUL_PROGRAM(1522)——真缺口。
// 先例：k_fullhp_immune_family 285（grant_immunity 票形）、k384 1725（ROUND_END
//   heal+账本门）、k168 363（DETECT 零状态吸血票）、k373 856（ROUND_START 异常门旗
//   每回合重写）、k_frost_round_fixed（deal_pink_damage/FIXED）。
// 场景回归：test/scenario/scenario_7420_fullhp_immune_healpink_1522.cpp（7420-7429）。

#include "common.h"

#include <primitives/battle_primitives.h>

namespace {

constexpr int kMarkId = 1522;
constexpr int kWiredBase = 990389000;      // 接线去重：+owner（grep 无撞）
constexpr int kNotFullKeyBase = 990389100; // ①"回合开始未满"门旗：+owner
constexpr int kOppAnomKeyBase = 990389200; // ②"回合开始对手异常"门旗：+owner

// ── 宿主行参数（方案 B：db_at 读 owned_int_args 原序，与真实对局一致）────────
struct HostRow {
    const std::vector<int>* db = nullptr;
    bool valid = false;
};

HostRow host_row(BattleContext* ctx, const SoulNodeArgs& a, int mark_id) {
    HostRow row;
    if (!ctx) {
        return row;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return row;
    }
    int slot = a.host_slot;
    if (slot < 0 || slot > 5) {
        slot = ctx->find_pet_with_soulmark(owner, mark_id);
    }
    if (slot < 0 || slot > 5) {
        return row;
    }
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int flag_of(BattleContext* ctx, int key) {
    const auto it = ctx->plugin_storage.find(key);
    return it == ctx->plugin_storage.end() ? 0 : any_int_or(it->second, 0);
}

// ── arm（enter/early）：接线 ② 吸血+条件等量固伤 DETECT 零状态票 ─────────────
EffectResult k389_arm(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    const int key = kWiredBase + owner;
    if (ctx->plugin_storage.count(key)) {
        return EffectResult::kOk;   // 双触发防重（K373 同款）
    }
    ctx->plugin_storage.emplace(key, 1);
    const HostRow row = host_row(ctx, a, kMarkId);
    const int pct = db_at(row, 1, 0);
    if (pct <= 0) {
        return EffectResult::kOk;   // 无参不接线（K1 纪律：不造 0% 票）
    }
    ctx->register_default_damage_effect(
        DamagePhase::DETECT, owner, DamageEffectCategory::DETECT,
        [owner, pct](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != owner) {
                return;
            }
            DamageSnapshot& d = c->resolvedDamage;
            if (d.attackerId != owner || !d.isRed || d.final <= 0) {
                return;
            }
            if (!onstage_has_soul_mark(c, owner, kMarkId)) {
                return;
            }
            const int amt = d.final * pct / 100;   // 吸血名义量（粉伤量同源）
            if (amt <= 0) {
                return;
            }
            if (g_core && g_core->heal_amount) {
                (void)g_core->heal_amount(c, owner, amt);
            }
            if (flag_of(c, kOppAnomKeyBase + owner) == 1 && g_core
                && g_core->deal_pink_damage) {
                (void)g_core->deal_pink_damage(c, 1 - owner, amt,
                                               DamageKind::FIXED, owner);
            }
        });
    return EffectResult::kOk;
}

// ── ROUND_START：① 满体→免异常票 / 未满→门旗；② 对手异常门旗（每回合无脑重写）─
EffectResult k389_round_start(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    ElfPet& self = ctx->getPet(owner);
    const bool alive = self.hp > 0;
    const int max_hp = alive ? resolve_pet_max_hp(self) : 0;
    const bool full = alive && self.hp >= max_hp;
    if (full) {
        ctx->grant_immunity(owner, ImmunityType::ANOMALY, coverage_all(),
                            /*anomaly_mask=*/0, /*duration_rounds=*/1,
                            /*source_id=*/152200, /*soul_immunity=*/true,
                            EffectScope::ON_STAGE, /*counts=*/0,
                            /*source_effect_id=*/kMarkId, ImmunityTier::Modern);
    }
    ctx->plugin_storage[kNotFullKeyBase + owner] = full ? 0 : 1;
    const int opp = 1 - owner;
    ctx->plugin_storage[kOppAnomKeyBase + owner] =
        ctx->has_any_active_abnormal_status(opp) ? 1 : 0;
    return EffectResult::kOk;
}

// ── ROUND_END：①' 未满旗开 → 恢复 max/db0 + 等量固伤（自伤面）────────────────
EffectResult k389_round_end(BattleContext* ctx, const SoulNodeArgs& a) {
    if (!ctx || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    const int owner = a.owner;
    if (!onstage_has_soul_mark(ctx, owner, kMarkId)) {
        return EffectResult::kOk;
    }
    if (flag_of(ctx, kNotFullKeyBase + owner) != 1) {
        return EffectResult::kOk;   // 回合开始满体 → 本分支不武装
    }
    ElfPet& self = ctx->getPet(owner);
    if (self.hp <= 0) {
        return EffectResult::kOk;   // 倒地不恢复（K384 同款）
    }
    const HostRow row = host_row(ctx, a, kMarkId);
    const int heal_denom = db_at(row, 0, -1);
    if (heal_denom <= 0) {
        return EffectResult::kOk;
    }
    (void)g_core->heal_amount(ctx, owner, resolve_pet_max_hp(self) / heal_denom);
    const int healed = ctx->ws.last_heal_amount[owner];   // 实际恢复量（封顶/封回血随缩）
    if (healed > 0 && g_core->deal_pink_damage) {
        (void)g_core->deal_pink_damage(ctx, owner, healed, DamageKind::FIXED, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +1（1522 入账，只增不改）──────
SOUL_PROGRAM(kMarkId, {SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k389_arm,
                             /*once=*/false, /*early=*/true, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::OPERATION_ENTER_EXIT_STAGE, &k389_round_start,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_START, &k389_round_start,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true),
                       SOUL_NODE(State::BATTLE_ROUND_END, &k389_round_end,
                             /*once=*/false, /*early=*/false, SoulScope::STAGE,
                             /*boss_invalid=*/true)});
