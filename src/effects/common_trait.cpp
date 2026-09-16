#include <effects/common_trait_effects.h>

#include <effects/trait_state.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

// ═══════════════════════════════════════════════════════════════════════════
// 通用特性（new_se stat=1）行为层。
//
// 安装形态与 install_default_* 一致：TEAM 绑定的常驻管线条目，**每方一个**，
// 回调里经 `effective_common_trait` 实时读**当前登场精灵**的生效特性——
// 切换/死亡天然正确（trait 没有就不做事），不需要 per-switch 重装。
// 查询/抑制/复制的可编程入口在 battleContext.h（TraitState 运行时层）。
//
// 机制口径（赛学必修6《精灵特性》+ 用户 2026-09-15/16 两轮实测拍板）：
//   - 瞬杀 **0-5 星统一 = 归零秒杀**（游戏更新后没有红伤修正了，星级只差触发概率）：
//     红伤结算完之后，经 `force_hp_to_zero` 原语强制对手体力归零。原语是秒杀族统一口子：
//     每次调用 emit EVENT_HP_TO_ZERO（blocked=是否被短路）；目标方"秒杀转化"标记或
//     来源方瞬杀特性被抑制 → 短路（咤克斯咒怨 +1 仍收得到事件）。犀牛回血挂
//     EVENT_TAKE_DAMAGE（drain 在状态桶之后）→ 回血晚于归零 → 高伤+瞬杀同时触发犀牛不死。
//   - 坚硬：靠前的乘法减伤，非通用减伤区、早于保底（L254），只免红伤；数值用语料修正表
//     （trait_hardness_pct，不读 DB args——官方读数错误）。
//   - 精神：特殊攻击伤害 +n%，非通用增伤 → 乘法（AMP_EXTRA 阶段），按 ws.skill_type_view 门控。
// ═══════════════════════════════════════════════════════════════════════════

namespace {

// 本次管线是否对应"攻击技能命中"的结算（瞬杀两变体的公共门）：
// 命中（HIT）才触发；miss/被封/命中失效都到不了管线（SKILL_INVALID 早退）或在此拦下。
bool instant_kill_gate(const BattleContext* ctx, int attacker_id) {
    if (ctx->ws.skill_exec_result[attacker_id] != SkillExecResult::HIT) {
        return false;   // 必修6：瞬杀需要攻击技能命中才能触发（0 星口径同，用户拍板）
    }
    if (ctx->ws.skill_type_view[attacker_id] == static_cast<int>(SkillType::Attribute)) {
        return false;   // 「进攻类技能」：物理/特殊可以，属性技能不行
    }
    return true;
}

//---- 瞬杀 0-5 星：红伤落地后的归零秒杀（钩位见 trait_instant_kill_zero_hook）----
// （旧"0 星红伤拉高"口径已废——游戏更新后 0-5 星统一为秒杀效果；TRAIT_REPLACE 阶段
//   已从管线移除，kOrder 常量恢复 11 阶段。）

//---- 坚硬：靠前的乘法减伤（REDUCE_TRAIT 阶段）----
void install_hardness(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::REDUCE_TRAIT,
        owner,
        DamageEffectCategory::MITIGATE,   // 减伤：不被"挡伤失效"无视（官方口径同减伤区）
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.defenderId != bucket_owner) {
                return;   // 减伤从"承受方"的桶读
            }
            if (!damage.isRed || damage.final <= 0) {
                return;   // 只免红伤（必修6：坚硬只能免减攻击伤害）
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::Hardness);
            if (!trait) {
                return;
            }
            const int pct = trait_hardness_pct(trait->star_level);
            if (pct <= 0) {
                return;
            }
            damage.final = damage.final * (100 - pct) / 100;   // 乘法（非通用减伤区）
        }
    );
}

//---- 精神：特殊攻击伤害 +n%（AMP_EXTRA 阶段，非通用增伤·乘法）----
void install_spirit(BattleContext* ctx, int owner) {
    ctx->register_default_damage_effect(
        DamagePhase::AMP_EXTRA,
        owner,
        DamageEffectCategory::AMP,
        [](BattleContext* c, int bucket_owner) {
            DamageSnapshot& damage = c->resolvedDamage;
            if (damage.attackerId != bucket_owner) {
                return;   // 增伤从"攻击方"的桶读
            }
            if (damage.final <= 0) {
                return;
            }
            const std::optional<EffectiveTrait> trait =
                c->effective_common_trait(bucket_owner, TraitKind::Spirit);
            if (!trait) {
                return;   // 先查特性（早退），再查技能类别
            }
            if (c->ws.skill_type_view[bucket_owner] != static_cast<int>(SkillType::Special)) {
                return;   // 「特殊攻击伤害增加」：只对特殊系技能
            }
            if (trait->args[1] == 0) {
                return;
            }
            damage.final = damage.final * (100 + trait->args[1]) / 100;
        }
    );
}

}  // namespace

void install_common_trait_effects(BattleContext* ctx) {
    if (!ctx) {
        return;
    }
    for (int owner = 0; owner < 2; ++owner) {
        install_hardness(ctx, owner);
        install_spirit(ctx, owner);
    }
}

void trait_instant_kill_zero_hook(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }
    if (!instant_kill_gate(ctx, attacker_id)) {
        return;
    }
    // ⚠️ include_suppressed=true：瞬杀被抑制 ≠ 不触发——照常掷点、照常调原语，
    //    抑制表现为原语短路（不归零、事件 blocked=true，咤克斯咒怨照样 +1）。
    const std::optional<EffectiveTrait> trait =
        ctx->effective_common_trait(attacker_id, TraitKind::InstantKill,
                                    /*include_suppressed=*/true);
    if (!trait) {
        return;
    }
    if (!trait_proc_roll(*trait)) {
        return;
    }
    const int defender_id = 1 - attacker_id;
    ElfPet& defender = ctx->seerRobot[defender_id].elfPets[ctx->on_stage[defender_id]];
    if (defender.hp <= 0) {
        return;
    }
    // 红伤已结算完（本钩位在 apply_resolved_damage 之后）→ 强制体力归零。
    // 犀牛式回血挂 EVENT_TAKE_DAMAGE、事件 drain 在状态桶之后 → 回血晚于归零，
    // 高伤+瞬杀同时触发时犀牛最后仍满血（用户 2026-09-15/16 实测口径）。
    (void)force_hp_to_zero(ctx, defender_id, attacker_id);
}

//---- 主动异常特性（静电/颤栗/火热/极寒，"主动毒"）：命中时施加给对手 ----
// 钩位：BATTLE_FIRST/SECOND_ON_SKILL_HIT 的注册桶之后、技能效果结算（SKILL_EFFECT）之前
// ——必修6："需要（自身）技能命中才可以触发"；被盔技能 SKILL_INVALID（不计为命中，
// 必修3）时 handler 连注册桶一起跳过，钩子自然不触发。
void trait_active_poison_hook(BattleContext* ctx, int attacker_id) {
    if (!ctx || attacker_id < 0 || attacker_id > 1) {
        return;
    }
    if (ctx->ws.skill_exec_result[attacker_id] != SkillExecResult::HIT) {
        return;   // 必修6 ②③：需要技能命中（miss 不触发）
    }
    const int skill_type = ctx->ws.skill_type_view[attacker_id];
    TraitKind want = TraitKind::None;
    if (skill_type == static_cast<int>(SkillType::Physical)) {
        want = TraitKind::ActivePoisonPhysical;   // 静电/颤栗：只认物理攻击
    } else if (skill_type == static_cast<int>(SkillType::Special)) {
        want = TraitKind::ActivePoisonSpecial;    // 火热/极寒：只认特殊攻击
    } else {
        return;   // 属性技能不触发（"物理攻击或者特殊攻击"）
    }
    std::optional<EffectiveTrait> trait = ctx->effective_common_trait(attacker_id, want);
    if (!trait) {
        // proc_forced（天女复制升级"取消攻击技能类型要求"）的复制条目可跨物/特类别触发：
        // 按本次技能类别没查到时，再查另一类别下的复制条目（本体条目不走这条路）。
        const TraitKind other_kind = (want == TraitKind::ActivePoisonPhysical)
                                         ? TraitKind::ActivePoisonSpecial
                                         : TraitKind::ActivePoisonPhysical;
        std::optional<EffectiveTrait> other =
            ctx->effective_common_trait(attacker_id, other_kind);
        if (other && other->copied && other->proc_forced) {
            trait = other;
        } else {
            return;
        }
    }
    if (!trait_proc_roll(*trait)) {
        return;
    }
    if (!is_valid_abnormal_status_id(trait->args[1])) {
        return;
    }
    const int defender_id = 1 - attacker_id;
    ElfPet& defender = ctx->seerRobot[defender_id].elfPets[ctx->on_stage[defender_id]];
    if (defender.hp <= 0) {
        return;
    }
    // 通道：本体条目 = 主动毒（古早通道：无视 Modern 免疫/弹控/抗性/转化）；
    // 天女式复制条目 = 现代通道（妙时天女专栏："通过此方式获得的异常特性所附加异常
    // 会走正常的免控流程"）。
    // 必修6：赋予回合随机 2~3。
    const int duration = 2 + std::rand() % 2;
    if (trait->copied) {
        (void)apply_anomaly(ctx, defender_id, trait->args[1], duration, attacker_id);
    } else {
        (void)apply_anomaly_ancient(ctx, defender_id, trait->args[1], duration, attacker_id);
    }
}
