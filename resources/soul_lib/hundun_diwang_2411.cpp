// 混沌地王莱茵哈特（神谕 3516）魂印 —— 封邪之嶂 / 留魂之蛊（精灵王线批 A，2026-09-24）
//
// 官方全文（effect_icon 1985 / effect 2411，args「1 0」）：
//   ① 自身护盾被击破时，附加对手最大体力1/3的百分比伤害且下次受到攻击的伤害额外减少50%；
//   ② 自身能力提升被消除、吸取时，自身获得1层封邪之嶂与400点护盾；
//   ③ 自身每次受到高于自身最大体力1/3的固定伤害、百分比伤害后，恢复等同于护盾值的体力；
//   ④ 回合开始时，若自身处于能力提升状态则为自身附加400点护盾，若自身不处于能力提升状态
//      则当回合战斗阶段结束时吸取对手最大体力的1/3并获得等量护盾，最高叠加至400点；
//   ⑤ 战斗阶段结束时，若自身的护盾值为400的正整数倍则为对手种下1只留魂之蛊；
//   （boss无效）
//
// 嶂的承伤 +10%/层（解析 idx=190 实测口径）走 ws.shield_absorb_bonus_pct（K2）；
// 蛊的"回血插入真伤"走 EVENT_HEAL_RESTORED（K3）；"强化被消除/吸取"走
// EVENT_STAT_REMOVED（K4）。监听方按用户口径做**在场守卫**（on_stage 是不是混地）。
// 口径备注（交接文档"待实测"清单同步）：
//   · ③ 的恢复量 = 当前护盾总值（DB 文本"等同于护盾值的体力"字面），非解析里的"1/3 吸血"。
//   · 蛊层数当前为 **对方 side 级** 计数（官方"对方每个精灵最多4层"的 per-pet 拆分待做）。
//   · "下次受到攻击伤害额外减少50%"用 damage_reduce_add 加算槽在对方行动期重写
//     （pending 旗在 EVENT_TAKE_DAMAGE 消费后立即清除）。
#include "common.h"
#include "../diwang_runtime.h"

namespace {

constexpr int kMark = 2411;

inline bool is_diwang_onstage(BattleContext* ctx, int owner) {
    return ctx->on_stage[owner] >= 0 && ctx->on_stage[owner] < 6
        && ctx->getPet(owner).soulMark.id == kMark;
}

// ── 嶂层数（per-pet 下场保留；发布槽同步给技能侧/承伤乘区）──
inline int zhang_layers(BattleContext* ctx, int owner) {
    return diwang_pub_get(ctx->plugin_storage, kDiwangPubZhang + owner);
}

inline void add_zhang(BattleContext* ctx, int owner, int n) {
    if (n <= 0) {
        return;
    }
    int layers = zhang_layers(ctx, owner) + n;
    if (layers > 4) {
        layers = 4;   // 官方印记上限 4（2319 参数与解析口径一致）
    }
    diwang_pub_set(ctx->plugin_storage, kDiwangPubZhang + owner, layers);
    // 承伤乘区：每层 +10%（ws 每回合 reset，这里实时重写 + ROUND_START 兜底重写）
    ctx->ws.shield_absorb_bonus_pct[owner] = 10 * layers;
}

// ── 蛊：种 1 层（挂在对方 side；cap 4）──
inline void plant_gu(BattleContext* ctx, int owner) {
    const int opp = 1 - owner;
    const int cur = diwang_pub_get(ctx->plugin_storage, kDiwangPubGu + opp);
    if (cur < 4) {
        diwang_pub_set(ctx->plugin_storage, kDiwangPubGu + opp, cur + 1);
    }
}

// ═══ 登场：挂全部 watcher + 兜底重写承伤乘区 ═══
EffectResult diwang_on_enter(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    auto& storage = ctx->getPet(owner).soulmark_storage;
    constexpr int kSlotKey = kMark * 100 + 91;
    if (storage.count(kSlotKey)) {
        return EffectResult::kOk;   // watcher 一次挂永久（TEAM）
    }
    storage[kSlotKey] = true;

    // ① 破盾：对手 max/3 百分比伤害 + 下次受攻伤减 50%（pending 旗）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_SHIELD_BROKEN, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || !is_diwang_onstage(wctx, owner)) {
                return;
            }
            if (g_core && g_core->deal_pink_damage) {
                const int max_hp = resolve_pet_max_hp(wctx->getPet(1 - owner));
                g_core->deal_pink_damage(wctx, 1 - owner, max_hp / 3,
                                         DamageKind::PERCENT_VALUE, owner);
            }
            wctx->ws.diwang_pending_reduce[owner] = true;
        },
        WatcherScope::TEAM);

    // ② 强化被消除/吸取（K4 事件）：+1 嶂 + 400 盾。在场守卫（用户口径）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_STAT_REMOVED, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || !is_diwang_onstage(wctx, owner)) {
                return;
            }
            add_zhang(wctx, owner, 1);
            wctx->getPet(owner).shield_bank_.add(ShieldBank::kSoulShieldPriority, 400,
                                                 /*source_id=*/kMark, /*duration=*/0);
        },
        WatcherScope::TEAM);

    // ③ 受高粉：恢复等同于护盾值的体力（0 血不触发；插入结算=事件 drain 里回血）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_TAKE_PINK_DAMAGE, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != owner || !is_diwang_onstage(wctx, owner)) {
                return;
            }
            ElfPet& me = wctx->getPet(owner);
            if (me.hp <= 0) {
                return;   // 默认 0 血不触发
            }
            const int threshold = resolve_pet_max_hp(me) / 3;
            if (ev.amount <= threshold) {
                return;
            }
            const int shield = me.shield_bank_.total();
            if (shield > 0 && g_core && g_core->heal_amount) {
                g_core->heal_amount(wctx, owner, shield);
            }
        },
        WatcherScope::TEAM);

    // 蛊：对方每次恢复体力 → 消耗 1 层插入 40 真伤（每回合 4 次上限，K3 事件）。
    ctx->event_center_.register_watcher(
        EventType::EVENT_HEAL_RESTORED, owner, ctx->roundCount, 0, /*once=*/false,
        [owner](BattleContext* wctx, const BattleEvent& ev) {
            if (!wctx || ev.target != 1 - owner || !is_diwang_onstage(wctx, owner)) {
                return;
            }
            const int opp = 1 - owner;
            int layers = diwang_pub_get(wctx->plugin_storage, kDiwangPubGu + opp);
            int used = diwang_pub_get(wctx->plugin_storage, kDiwangPubGuCap + opp);
            // 回合界：cap 计数带回合戳，换回合清零（存高 16 位 = 回合数）
            auto it = wctx->plugin_storage.find(kDiwangPubGuCap + opp + 100);
            const int stamp = it != wctx->plugin_storage.end()
                                  ? any_int_or(it->second) : -1;
            if (stamp != wctx->roundCount) {
                used = 0;
                diwang_pub_set(wctx->plugin_storage, kDiwangPubGuCap + opp, 0);
                diwang_pub_set(wctx->plugin_storage, kDiwangPubGuCap + opp + 100,
                               wctx->roundCount);
            }
            if (layers <= 0 || used >= kDiwangGuRoundCap) {
                return;
            }
            // 消耗 1 层 → 插入真伤（"先回血，然后中途插入真伤扣血"——事件在回血落地后 drain）
            --layers;
            diwang_pub_set(wctx->plugin_storage, kDiwangPubGu + opp, layers);
            diwang_pub_set(wctx->plugin_storage, kDiwangPubGuCap + opp, used + 1);
            if (g_core && g_core->deal_true_damage) {
                g_core->deal_true_damage(wctx, ev.target, kDiwangGuHealDmg, owner);
            }
        },
        WatcherScope::TEAM);
    return EffectResult::kOk;
}

// ═══ 回合开始：有提升 → +400 盾；无提升 → 授旗（阶段末吸取分支）═══
EffectResult diwang_round_start(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (me.soulMark.id != kMark) {
        return EffectResult::kOk;
    }
    bool boosted = false;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            boosted = true;
            break;
        }
    }
    if (boosted) {
        me.shield_bank_.add(ShieldBank::kSoulShieldPriority, kDiwangShieldUnit,
                            /*source_id=*/kMark, /*duration=*/0);
    }
    ctx->ws.diwang_phase_drain_arm[owner] = !boosted;
    // 嶂的承伤乘区兜底重写（ws 已被 reset）
    ctx->ws.shield_absorb_bonus_pct[owner] = 10 * zhang_layers(ctx, owner);
    // 破盾补偿的"下次受攻伤减 50%"：pending 旗 → 本回合加算减伤槽写 50（近似"下次"，
    // 精确的一次性语义见交接文档待办），在阶段末消费后清旗。
    if (ctx->ws.diwang_pending_reduce[owner]) {
        ctx->ws.damage_reduce_add[owner][0] += 50;
    }
    return EffectResult::kOk;
}

// ═══ 战斗阶段结束：无提升分支吸取；盾值 = 400 正整数倍 → 种 1 蛊 ═══
EffectResult diwang_phase_end(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    ElfPet& me = ctx->getPet(owner);
    if (me.soulMark.id != kMark) {
        return EffectResult::kOk;
    }
    if (ctx->ws.diwang_phase_drain_arm[owner] && g_core && g_core->drain_hp) {
        // 吸取对手最大体力 1/3 并获得等量护盾（"最高叠加至400点"按本盾笔钳制）。
        // 等量基准 = 对手实际被吸掉的体力（ heal 侧有自身体力上限封顶，不能用作盾基数）。
        ElfPet& opp_pet = ctx->getPet(1 - owner);
        const int opp_before = opp_pet.hp;
        g_core->drain_hp(ctx, owner, 1 - owner, /*fraction_denom=*/3);
        const int drained = opp_before - opp_pet.hp;
        if (drained > 0) {
            const int cur = me.shield_bank_.quantity_of(kMark);
            const int add = (cur + drained > kDiwangShieldUnit) ? kDiwangShieldUnit - cur
                                                                : drained;
            if (add > 0) {
                me.shield_bank_.add(ShieldBank::kSoulShieldPriority, add,
                                    /*source_id=*/kMark, /*duration=*/0);
            }
        }
    }
    ctx->ws.diwang_phase_drain_arm[owner] = false;
    if (ctx->ws.diwang_pending_reduce[owner]) {
        ctx->ws.damage_reduce_add[owner][0] -= 50;
        ctx->ws.diwang_pending_reduce[owner] = false;
    }
    // ⑤ 盾值 = 400 的正整数倍 → 种 1 只蛊
    const int total = me.shield_bank_.total();
    if (total > 0 && total % kDiwangShieldUnit == 0) {
        plant_gu(ctx, owner);
    }
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(2411, {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &diwang_on_enter,
                     /*once=*/false, /*early=*/false),
                    SoulMarkNodeRef(State::BATTLE_ROUND_START, &diwang_round_start),
                    SoulMarkNodeRef(State::BATTLE_ROUND_END, &diwang_phase_end)});
