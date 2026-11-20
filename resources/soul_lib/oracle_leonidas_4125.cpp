// 神谕古王（洪荒·列奥尼达神谕 4125）魂印 —— 旧日之晷 / 宙变之殢
//
// 官方判定（语料 idx=119《机制解析—洪荒·列奥尼达（神谕）》，魂印原文以引号内为据）：
//   ① "自身使用没有上述效果的技能后获得1层旧日之晷" —— 本次登场首次使用的技能给 1 层；
//      另一途径：150（2400）消除自身当回合使用技能的记录 → 下次使用技能后再拿 1 层。
//      日晷到 5 层后，后续再获得不算"获得日晷"（影响 ③ 的分支判定）。
//   ② "回合开始时，若场上存在异常状态……" —— 双方任意一方处于异常：自身获 1/3 最大体力
//      护盾 + 附加 2 层宙变之殢；双方均不处于异常：战斗阶段结束时**双方**恢复各自当前
//      护盾值的体力。
//   ③ "回合开始时，若本次在场没有获得日晷印记则战斗阶段结束时附加1回合衰弱，未触发就
//      获得3层宙变之殢" —— 没拿过日晷 → 阶段末给对手挂 1 回合衰弱；拿过 → 3 层殢。
//   ④ "拥有该印记时令对手pp值为1的技能和与之类型相同的技能无效……每次在场限定触发
//      n次，n为该印记层数，至多可以触发10次" —— 实时结算（先手清 pp 也能达成拿盔）→
//      封属票 + 动态条件（判定时读对手当前所选技能 pp）；触发预算 = 层数（至多 10）。
//   ⑤ "回合开始时，自身pp损失≥12点则回满血和pp，每有1层每次在场可以触发1次" ——
//      预算 = 登场时的层数（本次在场），每回合至多 1 次；阈值 12 被 2401 每消逝 -1。
//   ⑥ "宙变之殢：绑定对手，下场不保留，最多5层。对手所处的异常回合数不会减少" ——
//      **冻结**（内核 anomaly_rounds_frozen：tick 延伸 + 2207 短路，见 battleContext.h）；
//      持有方（对手）切换下场即清零；每有 1 层，自身本次在场的前 N 回合造成的非真实
//      伤害减少 50%（粉伤半边=AMP 管线乘 0.5^层，红伤半边=ROUND_START 写 damage_add_pct）。
//   "旧日之晷/宙变之殢中的本次在场，均为下场重置记录" —— 在场标志在登场节点重置。
//
// 层数真相源 = int 发布槽（kOraclePubSundial/kOraclePubZhie，跨 dylib 安全），
// 见 resources/oracle_runtime.h。
#include "common.h"
#include "../oracle_runtime.h"

namespace {

// 神谕态注册键 = effect_icon 神谕版行（icon 2051）的 effect_id 2470；
// 未神谕 = 低版本行（icon 1332）effect_id 1715（神谕前"荒"，未做）。
// PetFactory 神谕开关按版本选行（K1，2026-09-24），真实装配路径从此能进本程序。
constexpr int kOracleMarkId = 2470;
constexpr int kWeaken = static_cast<int>(AbnormalStatusId::Weakness);  // 衰弱 11
constexpr int kShieldSource = 41250;                                   // 魂印盾 source_id
constexpr int kLockSource = 41251;                                     // pp1 锁封属 effect_id

OracleRuntime& oracle_rt(BattleContext* ctx, int side) {
    const int key = kOracleMarkId * 100 + side;
    auto& storage = ctx->plugin_storage;
    auto it = storage.find(key);
    if (it == storage.end()) {
        it = storage.emplace(key, OracleRuntime{}).first;
    }
    return std::any_cast<OracleRuntime&>(it->second);
}

int sundial_layers(BattleContext* ctx, int side) {
    return oracle_pub_get(ctx->plugin_storage, kOraclePubSundial + side);
}

int zhie_layers(BattleContext* ctx, int holder_side) {
    return oracle_pub_get(ctx->plugin_storage, kOraclePubZhie + holder_side);
}

void zhie_set(BattleContext* ctx, int holder_side, int layers) {
    const int clamped = layers < 0 ? 0 : (layers > 5 ? 5 : layers);
    oracle_pub_set(ctx->plugin_storage, kOraclePubZhie + holder_side, clamped);
    oracle_sync_freeze(ctx, holder_side, clamped);
}

void zhie_add(BattleContext* ctx, int holder_side, int n) {
    zhie_set(ctx, holder_side, zhie_layers(ctx, holder_side) + n);
}

// 对手（非古王方）身上是否有任意活跃异常
bool side_has_any_anomaly(BattleContext* ctx, int side) {
    if (!g_core || !g_core->has_active_anomaly) {
        return false;
    }
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (g_core->has_active_anomaly(ctx, side, id)) {
            return true;
        }
    }
    return false;
}

// ── ④ pp=1 锁：封属票 + 动态条件（判定时读对手当前所选技能；实时结算）──
void grant_pp1_lock(BattleContext* ctx, int side) {
    auto cond = [side](BattleContext* c, int user, int, bool) -> bool {
        if (!c || user != 1 - side) {
            return false;
        }
        OracleRuntime& st = oracle_rt(c, side);
        const int budget = std::min(sundial_layers(c, side), 10);
        if (st.lock_triggers_used >= budget) {
            return false;
        }
        if (c->roundChoice[user][0] != 1 /*SELECT_SKILL*/) {
            return false;
        }
        const int idx = c->roundChoice[user][1];
        ElfPet& offender = c->seerRobot[user].elfPets[c->on_stage[user]];
        if (idx < 0 || idx >= static_cast<int>(offender.skills.size())) {
            return false;
        }
        const Skills& chosen = offender.skills[static_cast<std::size_t>(idx)];
        const bool pp1 = (chosen.pp == 1);
        bool same_type = false;
        if (!pp1) {
            // "与之类型相同的技能"：存在 pp==1 的同类型技能 → 该类型全体无效
            for (const Skills& sk : offender.skills) {
                if (sk.pp == 1 && sk.type == chosen.type) {
                    same_type = true;
                    break;
                }
            }
        }
        if (!pp1 && !same_type) {
            return false;
        }
        ++st.lock_triggers_used;
        return true;
    };
    // 回合型（duration=1，ROUND_START 逐回合续窗）；条件票（seal_skill 不透传动态条件，
    // 直接走 RuleCenter）。覆盖键 (side, kLockSource, SEAL, SEAL_ALL) 刷新不追加。
    ctx->rule_center_.grant_seal(
        /*source_owner=*/side, /*source_slot=*/-1, /*source_effect_id=*/kLockSource,
        /*target=*/1 - side, SealKind::SEAL_ALL, /*counts=*/0, /*rounds=*/1,
        /*penetrable=*/true, EffectScope::ON_STAGE, std::move(cond),
        /*source_valid_id=*/0, /*consumed_when_pierced=*/false);
}

// ── 节点①：ENTER_EXIT_STAGE（early, STAGE）—— 登场重置在场标志 + 常驻监听 ──
EffectResult oracle_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    OracleRuntime& st = oracle_rt(ctx, side);
    // "本次在场"记录全部重置（下场重置，重新登场再次触发）
    st.first_use_consumed = false;
    st.gained_this_presence = false;
    st.pp_restore_budget = sundial_layers(ctx, side);
    st.pp_restored_this_round = false;
    st.lock_triggers_used = 0;
    st.arm_heal_phase_end = false;
    st.arm_weaken_phase_end = false;
    st.enter_round = ctx->roundCount;
    grant_pp1_lock(ctx, side);

    // ⑥ 粉伤半边（非真实伤害含固定/百分比）：AMP 阶段按当前层数乘 (1 - 50%×层)
    ctx->register_pink_damage_effect(
        PinkDamagePhase::AMP, side,
        [side](BattleContext* c, int bucket_owner) {
            if (!c || bucket_owner != side || c->resolvedPink.actor != side) {
                return;
            }
            const int layers = oracle_pub_get(c->plugin_storage, kOraclePubZhie + (1 - side));
            if (layers <= 0 || c->resolvedPink.final <= 0) {
                return;
            }
            // 古王作为**来源方**的桶被走两遍（攻/防各一）——只在来源=古王侧施加
            c->resolvedPink.final -= c->resolvedPink.final * 50 * layers / 100;
        },
        EffectScope::TEAM);

    if (!st.watchers_registered) {
        st.watchers_registered = true;
        // 对手切换下场 → 宙变之殢清零（"绑定对手，下场不保留"）
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, side, ctx->roundCount, 0, false,
            [side](BattleContext* c, const BattleEvent& ev) {
                if (ev.actor == 1 - side) {
                    zhie_set(c, 1 - side, 0);
                }
            },
            WatcherScope::TEAM);
        // 双方每消逝 1 只 → 计数 +1（2401 的阈值-1 / 增伤+30%）。
        // 按 side 分键：镜像对战（双方古王）时各自计自己的份，不会双计。
        ctx->event_center_.register_watcher(
            EventType::EVENT_VANISH, side, ctx->roundCount, 0, false,
            [side](BattleContext* c, const BattleEvent&) {
                oracle_pub_set(c->plugin_storage, kOraclePubVanish + side,
                               oracle_pub_get(c->plugin_storage, kOraclePubVanish + side) + 1);
            },
            WatcherScope::TEAM);
    }
    return EffectResult::kOk;
}

// ── 节点②：ROUND_START（STAGE）—— pp 回满 / ②③ 分支 / pp1 锁续窗 ──
EffectResult oracle_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    OracleRuntime& st = oracle_rt(ctx, side);
    ElfPet& host = ctx->seerRobot[side].elfPets[ctx->on_stage[side]];
    if (host.soulMark.id != kOracleMarkId) {
        return EffectResult::kOk;
    }
    st.pp_restored_this_round = false;

    // ⑥ 宙变之殢的红伤半边：每有 1 层，本次在场的前 N 回合造成的非真实伤害 -50%
    //    （红伤半边走加算减伤槽；粉伤半边在 enter 注册的 AMP 管线条目）。ws 每回合
    //    reset → 每回合重写。N = 当前殢层数，"前 N 回合"按本次登场起算。
    {
        const int layers = zhie_layers(ctx, 1 - side);
        if (layers > 0 && ctx->roundCount - st.enter_round < layers) {
            ctx->ws.damage_add_pct[side] -= 50 * layers;
        }
    }

    // ⑤ pp 损失 ≥ 阈值（12 - 消逝数）→ 回满血和 pp（每层 1 次/在场，每回合 1 次）
    int pp_loss = 0;
    for (const Skills& sk : host.skills) {
        if (sk.pp >= 0 && sk.maxPP > 0) {
            pp_loss += std::max(0, sk.maxPP - sk.pp);
        }
    }
    const int threshold = std::max(0, 12 - oracle_pub_get(ctx->plugin_storage,
                                                          kOraclePubVanish + side));
    if (pp_loss >= threshold && st.pp_restore_budget > 0 && !st.pp_restored_this_round) {
        --st.pp_restore_budget;
        st.pp_restored_this_round = true;
        host.hp = resolve_pet_max_hp(host);
        for (Skills& sk : host.skills) {
            if (sk.pp >= 0 && sk.maxPP > 0) {
                sk.pp = sk.maxPP;
            }
        }
    }

    // ② 场上有异常 → 1/3 护盾 + 2 层殢；否则阶段末双方按护盾值回血
    const bool field_anomaly =
        side_has_any_anomaly(ctx, 0) || side_has_any_anomaly(ctx, 1);
    if (field_anomaly) {
        const int shield = std::max(1, resolve_pet_max_hp(host) / 3);
        host.shield_bank_.add(ShieldBank::kSoulShieldPriority, shield, kShieldSource, /*duration=*/1);
        zhie_add(ctx, 1 - side, 2);
    } else {
        st.arm_heal_phase_end = true;
    }

    // ③ 本次在场没拿过日晷 → 阶段末给对手 1 回合衰弱；拿过 → 3 层殢
    if (!st.gained_this_presence) {
        st.arm_weaken_phase_end = true;
    } else {
        zhie_add(ctx, 1 - side, 3);
    }

    grant_pp1_lock(ctx, side);   // 续窗
    return EffectResult::kOk;
}

// ── 节点③：ROUND_END（STAGE）—— 阶段末收尾：双方按护盾值回血 / 对手衰弱 ──
EffectResult oracle_round_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    OracleRuntime& st = oracle_rt(ctx, side);
    if (st.arm_heal_phase_end) {
        st.arm_heal_phase_end = false;
        for (int s = 0; s < 2; ++s) {
            ElfPet& pet = ctx->seerRobot[s].elfPets[ctx->on_stage[s]];
            const int shield = pet.shield_bank_.total();
            if (shield > 0 && g_core && g_core->heal_amount) {
                g_core->heal_amount(ctx, s, shield);
            }
        }
    }
    if (st.arm_weaken_phase_end) {
        st.arm_weaken_phase_end = false;
        if (g_core && g_core->apply_anomaly) {
            // ⚠️ 回合末附着 +1：ROUND_END 时 roundCount 还停在当回合，"1 回合"要
            //    从下一回合起算 → duration 传 2（derive_anomaly 同款 convention）。
            g_core->apply_anomaly(ctx, 1 - side, kWeaken, /*duration=*/1 + 1, /*actor=*/side);
        }
    }
    return EffectResult::kOk;
}

// ── 节点④：AFTER_ACTION（STAGE）—— 首用层 / 2400 清记录后的再拿 ──
EffectResult oracle_after_action(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1) {
        return EffectResult::kOk;
    }
    OracleRuntime& st = oracle_rt(ctx, side);
    if (ctx->roundChoice[side][0] != 1 /*SELECT_SKILL*/) {
        return EffectResult::kOk;
    }
    // 2400 清记录请求 → 本次行动不拿层，重置标志（"下次使用技能后可以获得日晷"）
    if (oracle_pub_get(ctx->plugin_storage, kOraclePubClearFirstUse + side) == 1) {
        oracle_pub_set(ctx->plugin_storage, kOraclePubClearFirstUse + side, 0);
        st.first_use_consumed = false;
        return EffectResult::kOk;
    }
    if (st.first_use_consumed) {
        return EffectResult::kOk;
    }
    st.first_use_consumed = true;
    const int layers = sundial_layers(ctx, side);
    if (layers < 5) {
        oracle_pub_set(ctx->plugin_storage, kOraclePubSundial + side, layers + 1);
        st.gained_this_presence = true;   // 满 5 层后的获得"不算获得日晷"
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kOracleMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &oracle_enter,
                              /*once=*/false, /*early=*/true, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &oracle_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &oracle_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_FIRST_AFTER_ACTION, &oracle_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_SECOND_AFTER_ACTION, &oracle_after_action,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
