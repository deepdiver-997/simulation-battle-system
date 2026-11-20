// 希拓 4622 魂印 —— 战意 / 命运庇护 / 烧伤闭环封锁
//
// 官方魂印（effect_icon 2217，4399 问答 992139）：
//   回合开始时100%令对手烧伤；
//   对手处于烧伤状态时无法恢复体力且每回合结束后受到战意层数*10的真实伤害；
//   回合结束后获得2层战意，己方场下每有1只系别中带有火系的精灵层数+1，战意达到10层时
//   获得命运庇护效果(层数下场保留，最多持有20层)；
//   战意：每持有一层受到的固定伤害、百分比伤害减少4%，造成的攻击伤害提升5%；
//   命运庇护：所有异常抗性必定触发，失去全免抗性(boss无效)。
//
// 关键口径（2026-09-20 烧伤三精灵线）：
//   ★ "对手处于烧伤状态"（锁疗 + 战意真伤）= **有效烧伤**（真实 ∪ 虚拟，
//     include/effects/burn_perception.h）——上古炎兽的"免弱→视为烧伤"同样被检测。
//   ★ 锁疗 = **条件型 HEAL_BLOCK 票**（RuleCenter 条件回调）：票长挂（TEAM 域、覆盖键
//     = 2217），条件回调实时查"对手侧有效烧伤"——烧伤在，heal 原语入口就被挡；烧伤
//     消失自然放行。希拓离场/阵亡由 on_exit 钩子撤票。IMMUNE 票天然免断回合/免 tick。
//   ★ 战意增伤/减伤走**管线实时读层数**（curse_stack.h 同款接线，零希拓对局零开销）：
//     攻击伤害 +5%/层（通用增伤·加法）；受到的固定/百分比伤害 -4%/层（粉伤管线减伤）。
//   ★ 战意层数存 soulmark_storage（下场保留 ✓ 官方"层数下场保留"），上限 20。
//   ★ 命运庇护 = 达 10 层时**一次性改写抗性数据**：已配置的抗性槽全部拉满 100%（必定
//     触发）、全免抗性清零。数据长在 pet 上，改写即持续（"boss无效"：引擎暂无 boss
//     概念，待 boss 标记落地后再补例外）。
//   ★ 回合结束定序：先用**当前**层数结算真伤，再获得新层数（文案顺序：真伤子句在前）。
#include "common.h"
#include <effects/burn_perception.h>
#include "../burn_trio_runtime.h"

namespace {

// 本回合该 side 是否真的点了技能（moves_lib 同款逻辑；soul 侧不共享那个头，就地复刻）。
bool chose_skill_this_round(const BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->roundChoice[side][0] == 1;
}

bool xitao_on_stage(BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->on_stage[side] >= 0
        && ctx->on_stage[side] < 6 && ctx->getPet(side).id == kXitaoPetId;
}

// ── 条件锁疗票：TEAM 域长挂、条件回调实时查有效烧伤；句柄存 plugin_storage 供撤销 ──
void grant_heal_block_ticket(BattleContext* ctx, int owner) {
    if (!ctx || !g_core) {
        return;
    }
    const int opp = 1 - owner;
    const int key = kXitaoHealBlockTicketBase + owner;
    int sid = 0;
    if (auto it = ctx->plugin_storage.find(key); it != ctx->plugin_storage.end()) {
        sid = any_int_or(it->second);
    }
    // 覆盖键 (owner=opp, effect_id=2217, IMMUNE, HEAL_BLOCK)：重授刷新不叠加。
    sid = ctx->rule_center_.grant_immune(
        opp, static_cast<int>(ImmunityType::HEAL_BLOCK), coverage_all(), /*anomaly_mask=*/0,
        /*duration_rounds=*/0, ctx->roundCount, sid, /*soul=*/false,
        EffectScope::TEAM, /*source_slot=*/-1, /*counts=*/0, /*source_effect_id=*/kXitaoMarkId,
        static_cast<int>(ImmunityTier::Modern),
        [opp](const BattleContext* c, int) { return is_burn_effective(c, opp); });
    ctx->plugin_storage[key] = sid;
}

// ── 管线接线（每场一次；实时读登场宠战意层数，非希拓宠读数为 0）──
void ensure_zhanyi_wiring(BattleContext* ctx) {
    if (!ctx || ctx->plugin_storage.count(kXitaoPipelineWiredKey)) {
        return;
    }
    ctx->plugin_storage[kXitaoPipelineWiredKey] = true;
    for (int owner = 0; owner < 2; ++owner) {
        // 造成的攻击伤害 +5%/层（通用增伤·加法）。
        ctx->register_default_damage_effect(
            DamagePhase::AMP, owner, DamageEffectCategory::AMP,
            [](BattleContext* c, int bucket_owner) {
                DamageSnapshot& d = c->resolvedDamage;
                if (d.attackerId != bucket_owner || !d.isRed || d.final <= 0) {
                    return;
                }
                const int n = xitao_zhanyi_stacks(c->getPet(bucket_owner));
                if (n > 0) {
                    d.addPct += n * 5;
                    d.final = d.final * (100 + n * 5) / 100;
                }
            });
        // 受到的固定伤害、百分比伤害 -4%/层（粉伤 REDUCE_PCT；真伤不走粉伤管线天然免掉）。
        ctx->register_default_pink_effect(
            PinkDamagePhase::REDUCE_EXTRA, owner,
            [](BattleContext* c, int bucket_owner) {
                PinkDamageResolved& r = c->resolvedPink;
                if (r.target != bucket_owner || r.final <= 0) {
                    return;
                }
                const int n = xitao_zhanyi_stacks(c->getPet(bucket_owner));
                if (n > 0) {
                    r.final = r.final * (100 - n * 4) / 100;
                    if (r.final < 0) {
                        r.final = 0;
                    }
                }
            });
    }
}

// ── 登场：接线 + 长挂锁疗票 ──
EffectResult xitao_enter(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !xitao_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    ensure_zhanyi_wiring(ctx);
    grant_heal_block_ticket(ctx, side);
    return EffectResult::kOk;
}

// ── 回合开始：100% 令对手烧伤（现代通道，免疫/弹控/抗性全检查）──
EffectResult xitao_round_start(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !xitao_on_stage(ctx, side) || !g_core
        || !g_core->apply_anomaly) {
        return EffectResult::kOk;
    }
    g_core->apply_anomaly(ctx, 1 - side, static_cast<int>(AbnormalStatusId::Burn),
                          /*duration=*/-1, side);
    return EffectResult::kOk;
}

// ── 回合结束：①烧伤真伤（当前层数*10）②获得战意 +2+火系场下数 ③达阈值得命运庇护 ──
EffectResult xitao_round_end(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !xitao_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    const int opp = 1 - side;
    // ① 对手处于（有效）烧伤 → 战意层数*10 真实伤害（真伤直通护盾/护罩）。
    if (is_burn_effective(ctx, opp) && g_core && g_core->deal_true_damage) {
        const int stacks = xitao_zhanyi_stacks(ctx->getPet(side));
        if (stacks > 0) {
            g_core->deal_true_damage(ctx, opp, stacks * 10, side);
        }
    }
    // ② 获得战意：+2，己方场下每有 1 只系别带火系的精灵再 +1（存活、不在场；
    //    消逝（上限归零）的剔掉）。
    ElfPet& host = ctx->getPet(side);
    int fire_off_field = 0;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[side]) {
            continue;
        }
        const ElfPet& pet = ctx->seerRobot[side].elfPets[slot];
        if (pet.hp > 0 && pet.numericalBase[NumericalPropertyIndex::HP] > 0
            && (pet.elementalAttributes[0] == kElementFire
                || pet.elementalAttributes[1] == kElementFire)) {
            ++fire_off_field;
        }
    }
    const int before = xitao_zhanyi_stacks(host);
    const int after = before + 2 + fire_off_field;
    xitao_set_zhanyi_stacks(host, after);
    // ③ 战意达到 10 层 → 命运庇护（一次性）。
    if (!xitao_has_destiny_guard(host) && after >= kZhanYiDestinyThreshold) {
        xitao_set_destiny_guard(host);
        xitao_apply_destiny_guard(host);
    }
    return EffectResult::kOk;
}

// 离场钩子：撤锁疗票（TEAM 域票不随换宠清，必须显式撤销）。
void xitao_exit(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return;
    }
    const int key = kXitaoHealBlockTicketBase + owner;
    if (auto it = ctx->plugin_storage.find(key); it != ctx->plugin_storage.end()) {
        ctx->revoke_immunity(owner, any_int_or(it->second));
        ctx->plugin_storage.erase(it);
    }
}

}  // namespace

SOUL_PROGRAM(kXitaoMarkId,
             {SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &xitao_enter,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_START, &xitao_round_start,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &xitao_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
SOUL_MARK_HOOKS(kXitaoMarkId, nullptr, &xitao_exit);
