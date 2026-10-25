// 时点状态快照实现（见 include/fsm/state_snapshot.h，决策 D16）。
//
// 规范化铁律：键序固定（按字段声明序显式枚举）、浮点 %.6g/%.9g 定格式、只含可变状态
// （静态配置如技能威力不入链——两侧平台必然一致，入链只会稀释漂移信号）、
// 指针/std::function/std::any 不入链。跨平台幂等是本机制的生命线。
#include <fsm/state_snapshot.h>

#include <fsm/battleContext.h>
#include <utils/sbs_rand.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace fsm {
namespace {

constexpr std::size_t kFnvOffset = 1469598103934665603ULL;
constexpr std::size_t kFnvPrime = 1099511628211ULL;

std::uint64_t fnv1a(const std::string& s) {
    std::uint64_t h = kFnvOffset;
    for (unsigned char c : s) {
        h ^= c;
        h *= kFnvPrime;
    }
    return h;
}

void append_file(const char* path, const std::string& data) {
    if (FILE* f = std::fopen(path, "ab")) {
        std::fwrite(data.data(), 1, data.size(), f);
        std::fclose(f);
    }
}

// ── 规范化文本构建器 ─────────────────────────────────────────────────────────
struct Canon {
    std::string s;
    void put(const char* k, long long v) {
        char buf[32];
        std::snprintf(buf, sizeof buf, "%s=%lld ", k, v);
        s += buf;
    }
    void putb(const char* k, bool v) { put(k, v ? 1 : 0); }
    void putf(const char* k, float v) {
        char buf[40];
        std::snprintf(buf, sizeof buf, "%s=%.6g ", k, static_cast<double>(v));
        s += buf;
    }
    void putd(const char* k, double v) {
        char buf[48];
        std::snprintf(buf, sizeof buf, "%s=%.9g ", k, v);
        s += buf;
    }
    void putraw(const char* k, const char* v) {
        s += k;
        s += '=';
        s += v;
        s += ' ';
    }
};  // Canon

}  // namespace (helpers)

namespace {

void put_damage_snapshot(Canon& c, const char* k, const DamageSnapshot& d) {
    char key[64];
    std::snprintf(key, sizeof key, "%s.atk", k); c.put(key, d.attackerId);
    std::snprintf(key, sizeof key, "%s.def", k); c.put(key, d.defenderId);
    std::snprintf(key, sizeof key, "%s.base", k); c.put(key, d.base);
    std::snprintf(key, sizeof key, "%s.afterAdd", k); c.put(key, d.afterAdd);
    std::snprintf(key, sizeof key, "%s.afterMul", k); c.put(key, d.afterMul);
    std::snprintf(key, sizeof key, "%s.final", k); c.put(key, d.final);
    std::snprintf(key, sizeof key, "%s.addPct", k); c.put(key, d.addPct);
    std::snprintf(key, sizeof key, "%s.mulCoef", k); c.putd(key, d.mulCoef);
    std::snprintf(key, sizeof key, "%s.isRed", k); c.putb(key, d.isRed);
    std::snprintf(key, sizeof key, "%s.isDirect", k); c.putb(key, d.isDirect);
    std::snprintf(key, sizeof key, "%s.isFixed", k); c.putb(key, d.isFixed);
    std::snprintf(key, sizeof key, "%s.isTrue", k); c.putb(key, d.isTrueDamage);
    std::snprintf(key, sizeof key, "%s.isWhite", k); c.putb(key, d.isWhiteNumber);
    std::snprintf(key, sizeof key, "%s.isCrit", k); c.putb(key, d.isCrit);
    std::snprintf(key, sizeof key, "%s.hitCount", k); c.put(key, d.hitCount);
    std::snprintf(key, sizeof key, "%s.skipRoutineBlock", k); c.putb(key, d.skip_routine_block);
    std::snprintf(key, sizeof key, "%s.attrElem", k);
    char pair[32];
    std::snprintf(pair, sizeof pair, "%d,%d", d.attribute_element[0], d.attribute_element[1]);
    char buf[96];
    std::snprintf(buf, sizeof buf, "%s=%s ", key, pair);
    c.s += buf;
}

void put_pink(Canon& c, const char* k, const PinkDamageResolved& p) {
    char key[64];
    std::snprintf(key, sizeof key, "%s.target", k); c.put(key, p.target);
    std::snprintf(key, sizeof key, "%s.actor", k); c.put(key, p.actor);
    std::snprintf(key, sizeof key, "%s.tier", k); c.put(key, static_cast<int>(p.tier));
    std::snprintf(key, sizeof key, "%s.raw", k); c.put(key, p.raw);
    std::snprintf(key, sizeof key, "%s.final", k); c.put(key, p.final);
    std::snprintf(key, sizeof key, "%s.absorbed", k); c.put(key, p.absorbed);
    std::snprintf(key, sizeof key, "%s.trueDmg", k); c.put(key, p.true_damage);
    std::snprintf(key, sizeof key, "%s.toHood", k); c.putb(key, p.to_hood);
}

void put_skill_view(Canon& c, const char* k, const SkillView& v) {
    char key[64];
    std::snprintf(key, sizeof key, "%s.value", k); c.put(key, v.value);
    std::snprintf(key, sizeof key, "%s.rw", k); c.putb(key, v.rewritten);
}

}  // namespace

void snapshot_record(const BattleContext& ctx, const std::string& phase) {
    static const bool enabled = std::getenv("SBS_SCENARIO_TRACE") != nullptr;
    if (!enabled) {
        return;
    }
    static const char* chain_path = std::getenv("SBS_SCENARIO_TRACE");
    static const char* full_path = std::getenv("SBS_SCENARIO_TRACE_FULL");

    Canon c;
    const BattleWorkspace& ws = ctx.ws;

    // ── 首要漂移探测器：全局 rand 状态 ──
    c.put("rand", static_cast<long long>(sbs_rand_state()));

    // ── FSM 标量 ──
    c.put("fsm.state", static_cast<int>(ctx.currentState));
    c.put("fsm.round", ctx.roundCount);
    c.put("fsm.cur", ctx.current_player_id_);
    c.putb("fsm.empty", ctx.is_empty);

    // ── workspace 全字段（声明序）──
    for (int i = 0; i < 2; ++i) {
        char key[64];
        for (int j = 0; j < 2; ++j) {
            std::snprintf(key, sizeof key, "ws.rc[%d][%d]", i, j);
            c.put(key, ws.roundChoice[i][j]);
        }
        std::snprintf(key, sizeof key, "ws.lastAct[%d]", i); c.put(key, ws.lastActionType[i]);
        std::snprintf(key, sizeof key, "ws.lastIdx[%d]", i); c.put(key, ws.lastActionIndex[i]);
        std::snprintf(key, sizeof key, "ws.preLvl[%d]", i); c.put(key, ws.preemptive_level[i]);
        std::snprintf(key, sizeof key, "ws.gFirst[%d]", i); c.put(key, ws.guaranteed_first[i]);
    }
    c.put("ws.preRight", static_cast<int>(ws.preemptive_right));
    put_damage_snapshot(c, "ws.pend", ws.pendingDamage);
    put_damage_snapshot(c, "ws.res", ws.resolvedDamage);
    put_pink(c, "ws.pink", ws.resolvedPink);
    for (int i = 0; i < 2; ++i) {
        char key[80];
        for (int j = 0; j < 8; ++j) {
            std::snprintf(key, sizeof key, "ws.asad.id[%d][%d]", i, j);
            c.put(key, ws.action_start_abnormal_damage_ids[i][j]);
            std::snprintf(key, sizeof key, "ws.asad.amt[%d][%d]", i, j);
            c.put(key, ws.action_start_abnormal_damage_amounts[i][j]);
        }
        std::snprintf(key, sizeof key, "ws.asad.cnt[%d]", i); c.put(key, ws.action_start_abnormal_damage_count[i]);
        std::snprintf(key, sizeof key, "ws.asad.pend[%d]", i); c.putb(key, ws.action_start_abnormal_damage_pending[i]);
        std::snprintf(key, sizeof key, "ws.anomOpp[%d]", i); c.putb(key, ws.anomaly_applied_by_opponent[i]);
        for (int j = 0; j < 4; ++j) {
            std::snprintf(key, sizeof key, "ws.redFlat[%d][%d]", i, j); c.put(key, ws.damage_reduce_flat[i][j]);
            std::snprintf(key, sizeof key, "ws.redAdd[%d][%d]", i, j); c.put(key, ws.damage_reduce_add[i][j]);
            std::snprintf(key, sizeof key, "ws.redMul[%d][%d]", i, j); c.put(key, ws.damage_reduce_mul[i][j]);
            std::snprintf(key, sizeof key, "ws.addExtra[%d][%d]", i, j); c.put(key, ws.damage_add_extra_mul[i][j]);
        }
        std::snprintf(key, sizeof key, "ws.dodge[%d]", i); c.putf(key, ws.dodge_rate[i]);
        std::snprintf(key, sizeof key, "ws.spdIg[%d]", i); c.put(key, ws.speed_ignore_points[i]);
        std::snprintf(key, sizeof key, "ws.defIg[%d]", i); c.put(key, ws.defense_ignore_pct[i]);
        std::snprintf(key, sizeof key, "ws.boostPierce[%d]", i); c.putb(key, ws.boost_pierced[i]);
        std::snprintf(key, sizeof key, "ws.hitMod[%d]", i); c.putf(key, ws.hit_rate_mod[i]);
        std::snprintf(key, sizeof key, "ws.hitModAtk[%d]", i); c.putf(key, ws.hit_rate_mod_attack[i]);
        std::snprintf(key, sizeof key, "ws.critMod[%d]", i); c.putf(key, ws.crit_rate_mod[i]);
        std::snprintf(key, sizeof key, "ws.critAdd[%d]", i); c.putf(key, ws.crit_rate_add[i]);
        std::snprintf(key, sizeof key, "ws.ikPerm[%d]", i); c.put(key, ws.instant_kill_permille_bonus[i]);
        std::snprintf(key, sizeof key, "ws.mustCrit[%d]", i); c.putb(key, ws.must_crit[i]);
        std::snprintf(key, sizeof key, "ws.attrMiss[%d]", i); c.putb(key, ws.attribute_must_miss[i]);
        std::snprintf(key, sizeof key, "ws.mustHitG[%d]", i); c.putb(key, ws.must_hit_grant[i]);
        std::snprintf(key, sizeof key, "ws.accFixG[%d]", i); c.putb(key, ws.accuracy_fix_grant[i]);
        std::snprintf(key, sizeof key, "ws.dmgAddPct[%d]", i); c.putf(key, ws.damage_add_pct[i]);
        std::snprintf(key, sizeof key, "ws.dmgAddFlat[%d]", i); c.put(key, ws.damage_add_flat[i]);
        std::snprintf(key, sizeof key, "ws.dmgFloor[%d]", i); c.put(key, ws.damage_floor[i]);
        for (int j = 0; j < 6; ++j) {
            std::snprintf(key, sizeof key, "ws.battr[%d][%d]", i, j); c.put(key, ws.battle_attrs[i].arr[j]);
            std::snprintf(key, sizeof key, "ws.vlvl[%d][%d]", i, j); c.put(key, ws.view_levels[i][j]);
        }
        std::snprintf(key, sizeof key, "ws.hitLvlX[%d]", i); c.put(key, ws.hit_level_extra[i]);
        std::snprintf(key, sizeof key, "ws.velem[%d][0]", i); c.put(key, ws.view_elementalAttributes[i][0]);
        std::snprintf(key, sizeof key, "ws.velem[%d][1]", i); c.put(key, ws.view_elementalAttributes[i][1]);
        std::snprintf(key, sizeof key, "ws.critRes[%d]", i); c.put(key, ws.eff_crit_resist_pct[i]);
        std::snprintf(key, sizeof key, "ws.fixRes[%d]", i); c.put(key, ws.eff_fixed_resist_pct[i]);
        std::snprintf(key, sizeof key, "ws.pctRes[%d]", i); c.put(key, ws.eff_percent_resist_pct[i]);
        std::snprintf(key, sizeof key, "ws.attacked[%d]", i); c.putb(key, ws.has_attacked[i]);
        std::snprintf(key, sizeof key, "ws.skillUsed[%d]", i); c.putb(key, ws.skill_used[i]);
        std::snprintf(key, sizeof key, "ws.extraPend[%d]", i); c.putb(key, ws.extra_action_pending[i]);
        std::snprintf(key, sizeof key, "ws.extraCnt[%d]", i); c.put(key, ws.extra_action_count[i]);
        std::snprintf(key, sizeof key, "ws.execRes[%d]", i); c.put(key, static_cast<int>(ws.skill_exec_result[i]));
        std::snprintf(key, sizeof key, "ws.srf[%d]", i);
        char pair[32];
        std::snprintf(pair, sizeof pair, "%d,%d",
                      ws.skill_resolution_flags[i].registerSkillEffects ? 1 : 0,
                      ws.skill_resolution_flags[i].allowAttackDamagePipeline ? 1 : 0);
        char buf[96];
        std::snprintf(buf, sizeof buf, "%s=%s ", key, pair);
        c.s += buf;
        std::snprintf(key, sizeof key, "ws.srReady[%d]", i); c.putb(key, ws.skill_resolution_ready[i]);
        std::snprintf(key, sizeof key, "ws.ppMult[%d]", i); c.put(key, ws.skill_pp_cost_multiplier[i]);
        std::snprintf(key, sizeof key, "ws.ppUsed[%d]", i); c.putb(key, ws.skill_pp_cost_consumed[i]);
        std::snprintf(key, sizeof key, "ws.cred.valid[%d]", i); c.putb(key, ws.attack_credential[i].valid);
        std::snprintf(key, sizeof key, "ws.cred.imm[%d]", i); c.putb(key, ws.attack_credential[i].ignore_attack_immunity);
        std::snprintf(key, sizeof key, "ws.cred.lim[%d]", i); c.putb(key, ws.attack_credential[i].ignore_damage_limit);
        std::snprintf(key, sizeof key, "ws.cred.force[%d]", i); c.putb(key, ws.attack_credential[i].force_execute);
        std::snprintf(key, sizeof key, "ws.cred.must[%d]", i); c.putb(key, ws.attack_credential[i].must_hit);
        std::snprintf(key, sizeof key, "ws.cred.acc100[%d]", i); c.putb(key, ws.attack_credential[i].accuracy_fix_100);
        std::snprintf(key, sizeof key, "ws.cred.lvl[%d]", i); c.put(key, ws.attack_credential[i].level);
        std::snprintf(key, sizeof key, "ws.igShield[%d]", i); c.putb(key, ws.ignore_shield[i]);
        std::snprintf(key, sizeof key, "ws.shieldAbs[%d]", i); c.put(key, ws.shield_absorb_bonus_pct[i]);
        std::snprintf(key, sizeof key, "ws.prioNull[%d]", i); c.putb(key, ws.priority_nullified[i]);
        std::snprintf(key, sizeof key, "ws.nutao[%d]", i); c.putb(key, ws.nutao_took_attack_damage[i]);
        std::snprintf(key, sizeof key, "ws.dwRed[%d]", i); c.putb(key, ws.diwang_pending_reduce[i]);
        std::snprintf(key, sizeof key, "ws.dwArm[%d]", i); c.putb(key, ws.diwang_phase_drain_arm[i]);
        std::snprintf(key, sizeof key, "ws.optElem[%d]", i); c.put(key, ws.optimal_elem_rounds[i]);
        std::snprintf(key, sizeof key, "ws.nnImm[%d]", i); c.putb(key, ws.nonnative_immune_armed[i]);
        std::snprintf(key, sizeof key, "ws.glaPar[%d]", i); c.putb(key, ws.gla_parity_flag[i]);
        std::snprintf(key, sizeof key, "ws.lastHeal[%d]", i); c.put(key, ws.last_heal_amount[i]);
        std::snprintf(key, sizeof key, "ws.rawDmg[%d]", i); c.put(key, ws.raw_attack_damage[i]);
        std::snprintf(key, sizeof key, "ws.powView[%d]", i); put_skill_view(c, key, ws.skill_power_view[i]);
        std::snprintf(key, sizeof key, "ws.combo[%d]", i); put_skill_view(c, key, ws.combo_view[i]);
        std::snprintf(key, sizeof key, "ws.useSeq0[%d]", i); c.put(key, ws.skill_use_seq_round_start[i]);
        std::snprintf(key, sizeof key, "ws.srcActive[%d]", i); c.putb(key, ws.skill_effect_source[i].active);
        std::snprintf(key, sizeof key, "ws.srcOwner[%d]", i); c.put(key, ws.skill_effect_source[i].source_owner);
        std::snprintf(key, sizeof key, "ws.srcSlot[%d]", i); c.put(key, ws.skill_effect_source[i].slot);
        std::snprintf(key, sizeof key, "ws.elemView[%d][0]", i); c.put(key, ws.skill_element_view[i][0]);
        std::snprintf(key, sizeof key, "ws.elemView[%d][1]", i); c.put(key, ws.skill_element_view[i][1]);
        std::snprintf(key, sizeof key, "ws.typeView[%d]", i); c.put(key, ws.skill_type_view[i]);
        std::snprintf(key, sizeof key, "ws.restraint[%d]", i); c.putd(key, ws.restraint_view[i]);
        std::snprintf(key, sizeof key, "ws.noWeak[%d]", i); c.putb(key, ws.no_weakness[i]);
        std::snprintf(key, sizeof key, "ws.elemGrant[%d][0]", i); c.put(key, ws.skill_element_grant[i][0]);
        std::snprintf(key, sizeof key, "ws.elemGrant[%d][1]", i); c.put(key, ws.skill_element_grant[i][1]);
        std::snprintf(key, sizeof key, "ws.elemGV[%d]", i); c.putb(key, ws.skill_element_grant_valid[i]);
        std::snprintf(key, sizeof key, "ws.guardDmg[%d]", i); c.put(key, ws.guard_detect_damage[i]);
        std::snprintf(key, sizeof key, "ws.guardHap[%d]", i); c.putb(key, ws.guard_detect_happened[i]);
        std::snprintf(key, sizeof key, "ws.forceInv[%d]", i); c.putb(key, ws.force_involve[i]);
        std::snprintf(key, sizeof key, "ws.hitInvMode[%d]", i); c.put(key, ws.hit_invalid_mode[i]);
        std::snprintf(key, sizeof key, "ws.hitInvDet[%d]", i); c.putb(key, ws.hit_invalid_detected[i]);
        std::snprintf(key, sizeof key, "ws.hitInvZero[%d]", i); c.putb(key, ws.hit_invalid_zero_damage[i]);
        std::snprintf(key, sizeof key, "ws.cachedSpd[%d]", i); c.put(key, ws.cached_speed[i]);
        std::snprintf(key, sizeof key, "ws.cachedCrit[%d]", i); c.put(key, ws.cached_crit_damage[i]);
    }
    c.put("ws.firstExhaust", ws.first_to_exhaust_foe);
    c.putb("ws.beforeHitAnchor", ws.before_hit_anchor_fired);

    // ── 上下文关键可变状态 ──
    for (int i = 0; i < 2; ++i) {
        char key[80];
        std::snprintf(key, sizeof key, "ctx.onStage[%d]", i); c.put(key, ctx.on_stage[i]);
        std::snprintf(key, sizeof key, "ctx.revid[%d]", i); c.put(key, ctx.round_effect_valid_id[i]);
        std::snprintf(key, sizeof key, "ctx.wvid[%d]", i); c.put(key, ctx.watcher_valid_id[i]);
        std::snprintf(key, sizeof key, "ctx.pvid[%d]", i); c.put(key, ctx.pipeline_valid_id[i]);
        std::snprintf(key, sizeof key, "ctx.hpZeroCv[%d]", i); c.putb(key, ctx.hp_zero_converted[i]);
        std::snprintf(key, sizeof key, "ctx.useSeq[%d]", i); c.put(key, ctx.skill_use_seq[i]);
        std::snprintf(key, sizeof key, "ctx.critHap[%d]", i); c.putb(key, ctx.crit_happened[i]);
        std::snprintf(key, sizeof key, "ctx.forceExec0[%d]", i); c.putb(key, ctx.force_execute_on_pp0[i]);
        std::snprintf(key, sizeof key, "ctx.igPP[%d]", i); c.putb(key, ctx.ignore_pp[i]);
        std::snprintf(key, sizeof key, "ctx.ppRev[%d]", i); c.putb(key, ctx.pp_reverse[i]);
        std::snprintf(key, sizeof key, "ctx.cannotAct[%d]", i); c.putb(key, ctx.cannot_act_round[i]);
        std::snprintf(key, sizeof key, "ctx.anomCap[%d]", i); c.put(key, ctx.anomaly_duration_cap[i]);
        for (int j = 0; j < 6; ++j) {
            std::snprintf(key, sizeof key, "ctx.abl[%d][%d]", i, j); c.put(key, ctx.ability_levels[i][j]);
        }
        for (int s = 0; s < kOfficialAbnormalStatusSlotCount; ++s) {
            std::snprintf(key, sizeof key, "ctx.anomEnd[%d][%d]", i, s);
            c.put(key, ctx.abnormal_status_end_round[i][s]);
        }
        std::snprintf(key, sizeof key, "ctx.elemView[%d][0]", i); c.put(key, ctx.elf_element_view[i][0]);
        std::snprintf(key, sizeof key, "ctx.elemView[%d][1]", i); c.put(key, ctx.elf_element_view[i][1]);
        std::snprintf(key, sizeof key, "ctx.elemBind[%d]", i); c.put(key, ctx.elf_element_view_bound_slot[i]);
        std::snprintf(key, sizeof key, "ctx.pinkRed[%d]", i); c.put(key, ctx.pink_reduce_pct[i]);
        std::snprintf(key, sizeof key, "ctx.healMod[%d]", i); c.put(key, ctx.heal_mod_pct[i]);
        std::snprintf(key, sizeof key, "ctx.ppRetain[%d]", i);
        char bits[24];
        std::snprintf(bits, sizeof bits, "%d%d%d%d%d%d",
                      ctx.pp_retain_pet[i][0] ? 1 : 0, ctx.pp_retain_pet[i][1] ? 1 : 0,
                      ctx.pp_retain_pet[i][2] ? 1 : 0, ctx.pp_retain_pet[i][3] ? 1 : 0,
                      ctx.pp_retain_pet[i][4] ? 1 : 0, ctx.pp_retain_pet[i][5] ? 1 : 0);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%s=%s ", key, bits);
        c.s += buf;
    }
    c.putb("ctx.blkOff", ctx.block_offstage_to_onstage);

    // ── 规则票（挡伤/免弱/封属——roll 结果的直接干预者）──
    {
        const auto& tickets = ctx.rule_center_.entries();
        c.put("tickets.n", static_cast<long long>(tickets.size()));
        for (std::size_t i = 0; i < tickets.size(); ++i) {
            char key[80];
            const auto& t = tickets[i];
            std::snprintf(key, sizeof key, "tk[%zu].own", i); c.put(key, t.source_owner);
            std::snprintf(key, sizeof key, "tk[%zu].slot", i); c.put(key, t.source_slot);
            std::snprintf(key, sizeof key, "tk[%zu].eid", i); c.put(key, t.source_effect_id);
            std::snprintf(key, sizeof key, "tk[%zu].sid", i); c.put(key, t.source_id);
            std::snprintf(key, sizeof key, "tk[%zu].vid", i); c.put(key, t.source_valid_id);
            std::snprintf(key, sizeof key, "tk[%zu].tgt", i); c.put(key, t.target);
            std::snprintf(key, sizeof key, "tk[%zu].sub", i); c.put(key, t.subtype);
            std::snprintf(key, sizeof key, "tk[%zu].soul", i); c.putb(key, t.soul);
            std::snprintf(key, sizeof key, "tk[%zu].tier", i); c.put(key, t.tier);
            std::snprintf(key, sizeof key, "tk[%zu].pen", i); c.putb(key, t.penetrable);
            std::snprintf(key, sizeof key, "tk[%zu].cwp", i); c.putb(key, t.consumed_when_pierced);
            std::snprintf(key, sizeof key, "tk[%zu].cnt", i); c.put(key, t.remaining_counts);
            std::snprintf(key, sizeof key, "tk[%zu].rnd", i); c.put(key, t.remaining_rounds);
        }
    }

    // ── 双方六宠必变状态（id/hp/盾罩/锁/系别/PP/技能锁）──
    for (int side = 0; side < 2; ++side) {
        for (int slot = 0; slot < 6; ++slot) {
            const ElfPet& pet = ctx.seerRobot[side].elfPets[slot];
            char key[80];
            std::snprintf(key, sizeof key, "pet[%d][%d].id", side, slot); c.put(key, pet.id);
            std::snprintf(key, sizeof key, "pet[%d][%d].hp", side, slot); c.put(key, pet.hp);
            std::snprintf(key, sizeof key, "pet[%d][%d].shield", side, slot); c.put(key, pet.shield);
            std::snprintf(key, sizeof key, "pet[%d][%d].cover", side, slot); c.put(key, pet.cover);
            std::snprintf(key, sizeof key, "pet[%d][%d].locked", side, slot); c.putb(key, pet.is_locked);
            std::snprintf(key, sizeof key, "pet[%d][%d].dsm", side, slot); c.put(key, pet.damage_suppress_mask);
            std::snprintf(key, sizeof key, "pet[%d][%d].spking", side, slot); c.putb(key, pet.is_spirit_king);
            std::snprintf(key, sizeof key, "pet[%d][%d].hpckit", side, slot); c.putb(key, pet.has_hp_consume_kit);
            std::snprintf(key, sizeof key, "pet[%d][%d].elem", side, slot);
            char pair[32];
            std::snprintf(pair, sizeof pair, "%d,%d", pet.elementalAttributes[0], pet.elementalAttributes[1]);
            c.putraw(key, pair);
            for (int s = 0; s < 5; ++s) {
                const Skills& sk = pet.skills[s];
                char key[80];
                std::snprintf(key, sizeof key, "pet[%d][%d].sk[%d]", side, slot, s);
                char trip[48];
                std::snprintf(trip, sizeof trip, "id=%d pp=%d lock=%d ",
                              sk.id, sk.pp, sk.is_locked ? 1 : 0);
                c.putraw(key, trip);
            }
        }
    }

    // ── 产出：链 hash（+ 可选全量文本）──
    const std::uint64_t h = fnv1a(c.s);
    char line[160];
    std::snprintf(line, sizeof line, "%016llx %s\n",
                  static_cast<unsigned long long>(h), phase.c_str());
    append_file(chain_path, line);
    if (full_path) {
        std::string block = "# ";
        block += phase;
        block += '\n';
        block += c.s;
        block += '\n';
        append_file(full_path, block);
    }
}

}  // namespace fsm
