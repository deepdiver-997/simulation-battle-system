#ifndef CURSE_STACK_H
#define CURSE_STACK_H

// ═══════════════════════════════════════════════════════════════════════════
// 魔王咒怨 —— **共享 buff 层**（不是某只精灵专属：湮灭之主·咤克斯 2378 与琉梦 2386
// 都可获得；以 4399 图鉴问答页原文为准，b 站专栏写的"5层免控"是过时口径）。
//
// 图鉴原文（reference/scrape_4399/out/hunyin.jsonl，pet_id 4762）：
//   「魔王咒怨：自身触发秒杀时，额外选择对方所有体力与原本秒杀目标相等的精灵为目标，
//     **达到10层时自身免疫控制类异常状态**；**每有1层受到的攻击伤害减少5%、造成的攻击
//     伤害提升5%、自身攻击有3%的概率造成的伤害不低于对手的最大体力值**」
//   · 减伤/增伤均为**通用**（L352 通用=加法）；
//   · 保底伤害「无需命中，只要使用攻击技能就触发」「乱穿犀牛、会正常受到减伤/挡伤/
//     护盾免减、不属于秒杀效果」——引擎落点 = 管线 **FLOOR** 阶段（减伤区之后、
//     犀牛检测之后、挡伤票之前）；盔/龙威/miss 出口走通用无效结算钩子。
//
// ⚠️ **本层完全在插件侧**（2026-09-16 用户拍板去耦合）：内核零咒怨知识——
//   层数存 pet.soulmark_storage（下场保留，pet 绑定）；增/减伤/FLOOR 是 ensure_curse_wiring
//   注册的**管线条目**（实时读登场宠层数，零咒怨对局零开销）；免控走 RuleCenter 票
//   （**登场检测器**：EVENT_ENTER_STAGE 时 ≥10 层就授，覆盖率比"越阈值瞬间授"好——
//   场下越过阈值后登场也能补上）。携带咒怨的技能/魂印在 arm 时调一次 ensure_curse_wiring。
// header-only：soul_lib/moves_lib 与测试共用；不链接任何 core 符号（只用内联入口）。
// ═══════════════════════════════════════════════════════════════════════════

#include <any>
#include <cstdint>
#include <map>

#include <abnormal-system/abnormal-types.h>
#include <fsm/battleContext.h>

// 每层数值（图鉴口径）。
constexpr int kCurseAmpPctPerStack = 5;     // 造成攻击伤害 +5%/层（通用增伤·加法）
constexpr int kCurseReducePctPerStack = 5;  // 受到攻击伤害 -5%/层（通用减伤·加法）
constexpr int kCurseFloorPctPerStack = 3;   // 攻击 3%/层概率保底"伤害不低于对手最大体力"
constexpr int kCurseControlImmuneStacks = 10;  // ★ 10 层（非 5 层）：免**控制类**异常

// ★ 10 层免控票的**档位与范围**（2026-09-28 官方实测定档 + 用户拍板）：
//   总口径 = **只免疫"现代通道 × 控制类"异常**：
//   · 档位 = **Modern 低级免控**（grant_immunity 默认档，两处授票都不传 tier）：
//     官方实测 10 层咒怨**仍会被主动毒（古早通道）控制**——古早施加只查 Ancient 层票，
//     本票对它不可见，正合实测。ImmunityTier"拿到实测翻"的第一个实锤样本（维持 Modern）。
//   · 范围 = **控制类掩码**（control_anomaly_mask()，battle_effects.Efftype=0 一族），
//     按官方文本"免疫控制类异常状态"字面执行——现代通道的**非控制类**异常
//     （中毒/烧伤等）**不免疫**、照常落地（056 Z5 已把三侧行为锁死）。
//   重授链路：票是 ON_STAGE（下场随 epoch 清）→ 再登场由 ⑤ 登场检测器重授
//   （层数 pet 绑定下场保留，≥10 层登场即恢复免控，无需新咒怨入手；056 Z5-③ 锁该链路）。

struct CurseState {
    int stacks = 0;
};

namespace curse_stack_detail {
    // pet.soulmark_storage 的咒怨槽（与魂印 mark_id*100+n 系键错开）。
    constexpr int kCurseStorageKey = 990237801;
    // ctx->plugin_storage 的接线守卫 / 保底掷点旗（插件自约定 key）。
    constexpr int kWiredFlagKey = 990237802;
    constexpr int kFloorFlagBase = 990237810;   // +side

    // 控制类异常掩码（battle_effects.Efftype=0 一族，见 is_control_abnormal_status）。
    // 免控票范围口径见 kCurseControlImmuneStacks 上方★注：只免控制类，非控制类不挡。
    inline uint64_t control_anomaly_mask() {
        uint64_t mask = 0;
        for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
            if (is_control_abnormal_status(static_cast<AbnormalStatusId>(id))) {
                mask |= (1ULL << id);
            }
        }
        return mask;
    }
}

// ── 层数存取（pet 绑定，下场保留）──────────────────────────────

// 取（必要时创建）指定 pet 槽的咒怨状态。咤"位于出战背包"时层数记在**咤自己那只
// pet** 上（可能不在场上），故必须按槽取。
inline CurseState& curse_state_of_slot(BattleContext* ctx, int side, int slot) {
    using namespace curse_stack_detail;
    auto& storage = ctx->seerRobot[side].elfPets[slot].soulmark_storage;
    auto it = storage.find(kCurseStorageKey);
    if (it == storage.end()) {
        storage.emplace(kCurseStorageKey, CurseState{});
        it = storage.find(kCurseStorageKey);
    }
    return std::any_cast<CurseState&>(it->second);
}

inline int get_curse_stacks_slot(BattleContext* ctx, int side, int slot) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5) {
        return 0;
    }
    return curse_state_of_slot(ctx, side, slot).stacks;
}

// 登场精灵（on_stage 槽）。
inline int get_curse_stacks(BattleContext* ctx, int side) {
    if (!ctx || side < 0 || side > 1) {
        return 0;
    }
    return get_curse_stacks_slot(ctx, side, ctx->on_stage[side]);
}

// 增加层数（指定槽）。越过 10 层阈值的瞬间，若该宠正在场上 → 授控制类免控票
// （场下越过则不授，等它登场时由 ENTER_STAGE 检测器补授）。
// 免控票 = RuleCenter ANOMALY + 控制类 mask，ON_STAGE（换宠随旧宠清，登场重授）。
inline void add_curse_stacks_slot(BattleContext* ctx, int side, int slot, int delta) {
    if (!ctx || side < 0 || side > 1 || slot < 0 || slot > 5 || delta <= 0) {
        return;
    }
    CurseState& cs = curse_state_of_slot(ctx, side, slot);
    const bool was_immune = cs.stacks >= kCurseControlImmuneStacks;
    cs.stacks += delta;
    if (!was_immune && cs.stacks >= kCurseControlImmuneStacks && ctx->on_stage[side] == slot) {
        // 档位/范围口径见 kCurseControlImmuneStacks 上方★注：Modern 默认档 + 控制类掩码。
        ctx->grant_immunity(side, ImmunityType::ANOMALY, /*coverage=*/~0ULL,
                           curse_stack_detail::control_anomaly_mask(),
                           /*duration_rounds=*/0,
                           /*source_id=*/curse_stack_detail::kCurseStorageKey,
                           /*soul_immunity=*/false, EffectScope::ON_STAGE);
    }
}

inline void add_curse_stacks(BattleContext* ctx, int side, int delta) {
    if (!ctx || side < 0 || side > 1) {
        return;
    }
    add_curse_stacks_slot(ctx, side, ctx->on_stage[side], delta);
}

// ── 接线（arm 时调一次；幂等）──────────────────────────────

/**
 * 安装魔王咒怨的全部战斗内检测器（幂等：plugin_storage 守卫，重复调用无害）。
 * 携带/授予咒怨的技能或魂印在**战斗开始钩子**里调用——零咒怨的对局零开销、内核零知识。
 * 注册四件套：
 *   ① AMP 管线条目（攻方桶）：+5%/层 通用增伤，实时读登场宠层数（含本回合新获得的）；
 *   ② REDUCE_PCT 管线条目（守方桶）：-5%/层 通用减伤（加法式乘算，钳负；只免红伤）；
 *   ③ FLOOR 管线条目（攻方桶）：3%/层 掷点命中 → 伤害抬到至少守方最大体力
 *      （命中路径的掷点；减伤先作用于公式值、犀牛检测在链首看不到、挡伤票随后仍可归零）；
 *   ④ 无效技能出口钩子（打盔/龙威/miss）：层数>0 且掷点命中 → 请求 FSM 重算结算一次
 *      （管线里的 ③ 自然生效）——图鉴「无需命中……有盔或者龙威并且没有穿透凭证也可以造成」；
 *   ⑤ 登场检测器（EVENT_ENTER_STAGE）：登场宠 ≥10 层 → 授控制类免控票
 *      （补上"场下越阈值后登场"的授票；on-stage 越阈值由 add_curse_stacks_slot 即时授）。
 */
inline void ensure_curse_wiring(BattleContext* ctx) {
    if (!ctx) {
        return;
    }
    using namespace curse_stack_detail;
    if (ctx->plugin_storage.count(kWiredFlagKey)) {
        return;   // 本场战斗已接线
    }
    ctx->plugin_storage[kWiredFlagKey] = true;

    // ①/②/③ 三条管线（TEAM 绑定；回调实时读"桶 owner 的登场宠"层数）。
    for (int owner = 0; owner < 2; ++owner) {
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || d.final <= 0) {
                    return;
                }
                const int n = get_curse_stacks(c, bucket_owner);
                if (n > 0) {
                    d.final = d.final * (100 + n * kCurseAmpPctPerStack) / 100;
                }
            });
        ctx->register_default_damage_effect(
            DamagePhase::REDUCE_PCT, owner, DamageEffectCategory::MITIGATE,
            [](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.defenderId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                const int n = get_curse_stacks(c, bucket_owner);
                if (n > 0) {
                    d.final = d.final * (100 - n * kCurseReducePctPerStack) / 100;
                    if (d.final < 0) {
                        d.final = 0;
                    }
                }
            });
        ctx->register_default_damage_effect(
            DamagePhase::FLOOR, owner, DamageEffectCategory::AMP,
            [](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || !d.isRed || d.final < 0) {
                    return;
                }
                // 掷点：无效出口钩子掷中时置旗（本条目消费）；正常命中路径在此掷。
                const int flag_key = kFloorFlagBase + bucket_owner;
                bool forced = false;
                if (c->plugin_storage.count(flag_key)) {
                    forced = std::any_cast<bool>(c->plugin_storage[flag_key]);
                    c->plugin_storage[flag_key] = false;   // 消费
                }
                if (!forced) {
                    const int n = get_curse_stacks(c, bucket_owner);
                    forced = n > 0 && (std::rand() % 100) < n * kCurseFloorPctPerStack;
                }
                if (!forced) {
                    return;
                }
                const int defender = d.defenderId;
                if (defender < 0 || defender > 1) {
                    return;
                }
                const int max_hp = c->seerRobot[defender].elfPets[c->on_stage[defender]]
                                     .numericalBase[NumericalPropertyIndex::HP];
                if (d.final < max_hp) {
                    d.final = max_hp;
                }
            });
    }

    // ④ 无效技能出口钩子：层数>0 且掷点命中 → 置旗 + 请求结算（FSM stage+finish，
    //    ③ 读旗落保底；不掷中就不请求——无效技能默认零伤害）。
    ctx->invalid_skill_damage_hooks.push_back(
        [](BattleContext* c, int attacker) -> bool {
            const int n = get_curse_stacks(c, attacker);
            if (n <= 0 || (std::rand() % 100) >= n * kCurseFloorPctPerStack) {
                return false;
            }
            c->plugin_storage[curse_stack_detail::kFloorFlagBase + attacker] = true;
            return true;
        });

    // ⑤ 登场检测器：登场宠 ≥10 层 → 授控制类免控票（ON_STAGE，换宠清、再登场重授）。
    for (int side = 0; side < 2; ++side) {
        ctx->event_center_.register_watcher(
            EventType::EVENT_ENTER_STAGE, side, ctx->roundCount, /*duration_rounds=*/0,
            /*once=*/false,
            [](BattleContext* wctx, const BattleEvent& ev) {
                if (!wctx || ev.actor < 0 || ev.actor > 1) {
                    return;
                }
                if (get_curse_stacks(wctx, ev.actor) < kCurseControlImmuneStacks) {
                    return;
                }
                // 档位/范围口径见 kCurseControlImmuneStacks 上方★注（控制类掩码）。
                wctx->grant_immunity(ev.actor, ImmunityType::ANOMALY, /*coverage=*/~0ULL,
                                    curse_stack_detail::control_anomaly_mask(),
                                    /*duration_rounds=*/0,
                                    /*source_id=*/curse_stack_detail::kCurseStorageKey,
                                    /*soul_immunity=*/false, EffectScope::ON_STAGE);
            },
            WatcherScope::TEAM);
    }
}

#endif // CURSE_STACK_H
