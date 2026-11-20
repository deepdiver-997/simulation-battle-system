// K12 条件增减伤/击杀成长族 —— effect 319/310/762（3 宏 / 5 行）
//
// 行清单（tools/soul_effects_dump.py 319 310 762，2026-10-05）：
//   319 咤克斯 2648 args[5, 10]（kind 缺）：
//     "出场时自身造成的伤害提升5%、受到的伤害降低5%；|在一场战斗中，每击败一只精灵
//     则自身造成的伤害提升10%、受到的伤害降低10%"
//     → ENTER 接线两张**零状态票**（yanshou 同款：管线回调实时读计数，无窗口无状态）：
//     AMP 桶（攻击==持有方）：d.addPct += 5+10×击杀数、final 同步乘算；
//     REDUCE_PCT 桶（防御==持有方）：final -= final×pct/100。
//     击杀计数 = BATTLE_AFTER_DEFEATING_OPPONENT 节点（桶按击败方配对——持有方击败
//     对手时触发），onstage 门 + hp<=0 归因核（find_defeated_foe_slot 同义）。
//   310 邪冥兽 2620 args[2, 50]：
//     "若每回合对手先出手，则50%使对手技能失效（boss无效）"
//     → db[0]=2 = **连续先手计数阈值**（工单"连击计数语义"现场解码记档）、db[1]=50。
//     FMR 节点：本回合先手权已结算（preemptive_right）——对手先手 → 连续计数 ++、
//     否则清零；计数 ≥ db[0] → 50% roll → seal_skill SEAL_ALL count=1 duration=1
//     hit_invalid=true（"技能失效"=命中失效语义，1527/J136 形）→ 计数清零。
//   762 四九圣尊 3106 args[49,49] / 3221 args[49,49,90,90,270]：
//     "回合开始时，自身体力高于对方则造成伤害增加49%，自身体力低于对方则受到的
//     伤害减少49%" → ENTER 接线两张零状态票（管线回调**实时比对双方 hp**——高体
//     AMP +49%、低体 REDUCE_PCT -49%，每回合自动重判，K5 1502 条件写槽同语义）。
//     ⚠️ 3221 的叠加段（90/90/270 吸取成长）tips 带 {吸取}{} 模板标注、语义未建模
//     ——按工单口径**记档跳过**。
//
// ⚠️ 三查记录（2026-10-05）：①内核无 319/310/762 魂印行为硬编码 ②effect_info 无此
//    三 id 技能模板 ③soul_lib 并集无同 id 注册——真缺口。
// 先例：yanshou_4875（AMP 零状态票 + register_default_damage_effect 双侧接线）、
//   batch_c 417-429（REDUCE_PCT MITIGATE 回调）、J136 1527（seal hit_invalid）、
//   J23 36（HIT 门）、K6 k_hit_suppress_cond（SOUL_PROGRAM 声明式节点形态）。
// 场景回归：test/scenario/scenario_2920_cond_dmg_stack.cpp（号段 2920-2929）。

#include "common.h"

#include <abnormal-system/abnormal-types.h>

namespace {

// ── 键段（990168xxx 族后续，grep 全库无撞）───────────────────────────────
constexpr int kWiredBase    = 990169000;   // +宏*10+owner：管线已接线旗
constexpr int k319Kills     = 990169100;   // +owner：319 击败计数
constexpr int k319Counted   = 990169150;   // +opp*8+slot：319 槽级已计旗
constexpr int k310Streak    = 990169200;   // +owner：310 对手连续先手计数
constexpr int k319Audit     = 990169300;   // +roundCount：319 本次申报 pct（F9 审计槽）

bool watcher_wired(BattleContext* ctx, int macro, int owner) {
    const auto key = static_cast<unsigned long long>(kWiredBase + macro * 10 + owner);
    if (ctx->plugin_storage.count(key)) {
        return true;
    }
    ctx->plugin_storage.emplace(key, 1);
    return false;
}

struct HostRow {
    const std::vector<int>* db = nullptr;
    int monster_id = 0;
    bool valid = false;
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
    row.db = &ctx->seerRobot[owner].elfPets[slot].soulMark.args.owned_int_args;
    row.monster_id = ctx->seerRobot[owner].elfPets[slot].soulMark.monster_id;
    row.valid = true;
    return row;
}

int db_at(const HostRow& row, int idx, int fallback) {
    if (row.db && idx >= 0 && idx < static_cast<int>(row.db->size())) {
        return (*row.db)[idx];
    }
    return fallback;
}

int owner_of(BattleContext* ctx, const EffectArgs& args) {
    return resolve_owner_from_args(ctx, args);
}

// ════════════════════ 319 咤克斯 ════════════════════

// ENTER 节点：接线 AMP + REDUCE_PCT 零状态票（双侧，回调实时读击杀计数）。
EffectResult k12_319_enter(BattleContext* ctx, const EffectArgs& args) {
    fprintf(stderr, "[DBG319] enter owner=%d\n", ctx ? resolve_owner_from_args(ctx, args) : -1);
    const int owner = owner_of(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 319, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 319);
    const int base = db_at(row, 0, 5);
    const int step = db_at(row, 1, 10);
    {
        const int side = owner;
        ctx->register_default_damage_effect(
            DamagePhase::AMP, side, DamageEffectCategory::AMP,
            [side, base, step](BattleContext* c, int bucket_owner) {
                if (bucket_owner != side) {
                    return;
                }
                const auto it =
                    c->plugin_storage.find(static_cast<unsigned long long>(k319Kills +
                                                                               side));
                const int kills =
                    it == c->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
                const int pct = base + step * kills;
                fprintf(stderr, "[DBG] amp319 side=%d kills=%d pct=%d rc=%d\n", side, kills,
                        pct, c->roundCount);
                if (pct <= 0) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != side || !d.isRed || d.final <= 0) {
                    return;
                }
                const unsigned long long wkey =
                    static_cast<unsigned long long>(k319Audit + c->roundCount);
                fprintf(stderr, "[DBG] amp write key=%llu val=%d\n", wkey, pct);
                c->plugin_storage[wkey] = pct;
                d.addPct += pct;
                d.final = d.final * (100 + pct) / 100;
            });
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, side, DamageEffectCategory::MITIGATE,
            [side, base, step](BattleContext* c, int bucket_owner) {
                if (bucket_owner != side) {
                    return;
                }
                const auto it =
                    c->plugin_storage.find(static_cast<unsigned long long>(k319Kills +
                                                                               side));
                const int kills =
                    it == c->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
                const int pct = base + step * kills;
                if (pct <= 0) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != side || !d.isRed || d.final <= 0) {
                    return;
                }
                d.final -= d.final * pct / 100;
            });
    }
    return EffectResult::kOk;
}

// AFTER_DEFEATED 节点（⚠️ 不是 AFTER_DEFEATING_OPPONENT——击杀后 dead1=true 时该
// 状态被 battleFsm:2213 跳过，实测节点永不触发，记档）：对手在场槽 hp<=0 且
// last_damage_actor 归因持有方 → 计数 ++（槽级幂等旗：复活/换入清旗可再计）。
EffectResult k12_319_after_defeated(BattleContext* ctx, const EffectArgs& args) {
    const int owner = owner_of(ctx, args);
    const int opp_dbg = 1 - (owner < 0 ? 0 : owner);
    const int slot_dbg = ctx ? ctx->on_stage[opp_dbg] : -1;
    fprintf(stderr, "[DBG319] after_def R=%d owner=%d slot=%d hp=%d actor=%d counted=%d\n",
            ctx ? ctx->roundCount : -1, owner, slot_dbg,
            ctx ? ctx->seerRobot[opp_dbg].elfPets[slot_dbg].hp : -1,
            ctx ? (int)ctx->last_damage_actor[opp_dbg][slot_dbg] : -1,
            ctx ? (int)ctx->plugin_storage.count(
                      static_cast<unsigned long long>(k319Counted + opp_dbg * 8 + slot_dbg))
                : -1);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 319)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const int slot = ctx->on_stage[opp];
    if (slot < 0 || slot > 5) {
        return EffectResult::kOk;
    }
    const auto counted_key = static_cast<unsigned long long>(k319Counted + opp * 8 + slot);
    if (ctx->seerRobot[opp].elfPets[slot].hp > 0) {
        ctx->plugin_storage.erase(counted_key);   // 该槽又有活口（换入/复活）→ 重新可计
        return EffectResult::kOk;
    }
    if (ctx->last_damage_actor[opp][slot] != owner) {
        return EffectResult::kOk;   // 非持有方击杀（归因）
    }
    if (ctx->plugin_storage.count(counted_key)) {
        return EffectResult::kOk;   // 已计过（幂等）
    }
    ctx->plugin_storage[counted_key] = 1;
    const auto key = static_cast<unsigned long long>(k319Kills + owner);
    const auto it = ctx->plugin_storage.find(key);
    const int cur = it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
    ctx->plugin_storage[key] = cur + 1;
    return EffectResult::kOk;
}

// ════════════════════ 310 邪冥兽 ════════════════════

// ROUND_COMPLETION 节点：本回合先手权已结算（pr 未重置）→ 连续计数。
// ⚠️ FMR 时点 pr 已被重置为 NONE（handle_BattleFirstMoveRight 先 memset 后跑桶），
//    计数必须在 RC 收口，记档。
EffectResult k12_310_rc(BattleContext* ctx, const EffectArgs& args) {
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 310)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const auto key = static_cast<unsigned long long>(k310Streak + owner);
    const auto it = ctx->plugin_storage.find(key);
    int streak = it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
    fprintf(stderr, "[DBG310] rc pr=%d owner=%d\n", (int)ctx->preemptive_right, owner);
    const bool opp_first = ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2
        ? opp == 1
        : opp == 0;
    if (ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_1) {
        streak = (owner == 0) ? streak + 1 : 0;
    } else if (ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2) {
        streak = (opp == 1) ? streak + 1 : 0;
    } else {
        streak = 0;
    }
    fprintf(stderr, "[DBG310] streak=%d\n", streak);
    ctx->plugin_storage[key] = streak;
    return EffectResult::kOk;
}

// FMR 节点：streak ≥ 阈值 → 50% 封技（hit_invalid）→ 清零。
EffectResult k12_310_fmr(BattleContext* ctx, const EffectArgs& args) {
    fprintf(stderr, "[DBG310] fmr enter\n");
    if (!g_core || !g_core->seal_skill) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1 || !onstage_has_soul_mark(ctx, owner, 310)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - owner;
    const HostRow row = host_row(ctx, args, 310);
    const int threshold = db_at(row, 0, 2);
    const int chance = db_at(row, 1, 50);
    const auto key = static_cast<unsigned long long>(k310Streak + owner);
    const auto it = ctx->plugin_storage.find(key);
    int streak = it == ctx->plugin_storage.end() ? 0 : std::any_cast<int>(it->second);
    fprintf(stderr, "[DBG310] fmr streak=%d threshold=%d\n", streak, threshold);
    if (streak < threshold || streak <= 0) {
        return EffectResult::kOk;
    }
    if (roll_percent(chance)) {
        (void)g_core->seal_skill(ctx, owner, opp, /*effect_id=*/310,
                                 /*attribute=*/true, /*attack=*/true,
                                 /*count=*/1, /*duration_rounds=*/1,
                                 /*penetrable=*/false, /*source_slot=*/-1,
                                 EffectScope::ON_STAGE, /*hit_invalid=*/true,
                                 /*chance_pct=*/100, /*consumed_when_pierced=*/false);
        streak = 0;   // 触发后清零（连续重新累计）
    }
    ctx->plugin_storage[key] = streak;
    return EffectResult::kOk;
}

// ════════════════════ 762 四九圣尊 ════════════════════

// ENTER 节点：接线零状态票（回调实时比对双方 hp——高体 AMP/低体 REDUCE_PCT）。
EffectResult k12_762_enter(BattleContext* ctx, const EffectArgs& args) {
    fprintf(stderr, "[DBG762] enter wired=%d\n",
            (int)ctx->plugin_storage.count(
                static_cast<unsigned long long>(kWiredBase + 762 * 10 +
                                                resolve_owner_from_args(ctx, args))));
    const int owner = owner_of(ctx, args);
    if (owner < 0 || owner > 1 || watcher_wired(ctx, 762, owner)) {
        return EffectResult::kOk;
    }
    const HostRow row = host_row(ctx, args, 762);
    const int amp_pct = db_at(row, 0, 49);
    const int reduce_pct = db_at(row, 1, 49);
    {
        const int side = owner;
        ctx->register_default_damage_effect(
            DamagePhase::AMP, side, DamageEffectCategory::AMP,
            [side, amp_pct](BattleContext* c, int bucket_owner) {
                if (bucket_owner != side) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != side || !d.isRed || d.final <= 0) {
                    return;
                }
                if (c->getPet(side).hp <= c->getPet(1 - side).hp) {
                    return;   // 高体门：自身体力**高于**对方
                }
                d.addPct += amp_pct;
                d.final = d.final * (100 + amp_pct) / 100;
            });
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, side, DamageEffectCategory::MITIGATE,
            [side, reduce_pct](BattleContext* c, int bucket_owner) {
                if (bucket_owner != side) {
                    return;
                }
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != side || !d.isRed || d.final <= 0) {
                    return;
                }
                if (c->getPet(side).hp >= c->getPet(1 - side).hp) {
                    return;   // 低体门：自身体力**低于**对方
                }
                d.final -= d.final * reduce_pct / 100;
            });
    }
    return EffectResult::kOk;
}

}  // namespace

// ── 效果注册（静态自注册）—— SOUL_PROGRAM 总数 +3（只增不改）────────────────
// 319 = ENTER 接线 + AFTER_DEFEATING 计数节点；310 = FMR 节点；762 = ENTER 接线。
// ⚠️ 2346 皮神同族行仅 boss 有效（PvE 专属）——不立单（工单口径记档）。
SOUL_PROGRAM(319, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k12_319_enter,
                    /*once=*/false, /*early=*/true),
                   SoulMarkNodeRef(State::BATTLE_AFTER_DEFEATED, &k12_319_after_defeated)});
SOUL_PROGRAM(310, {SoulMarkNodeRef(State::BATTLE_ROUND_COMPLETION, &k12_310_rc),
                   SoulMarkNodeRef(State::BATTLE_FIRST_MOVE_RIGHT, &k12_310_fmr)});
SOUL_PROGRAM(762, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &k12_762_enter,
                    /*once=*/false, /*early=*/true)});
