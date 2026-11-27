// 极渊DS-001 4797（effect_icon 1987 / effect_id 2414）—— 算力 / 算力乱流 / 算力共振
//
// 口径（数据库 effect_icon 1987 tips 全文 + 《机制解析—极渊DS-001》opus 1096017478617661460，
//       详证见 docs_local/docs/05-任务清单/开工-极渊DS-001与无极圣武.md §一）：
//   「登场时，自身获得100点算力并获得算力限制，之后每回合开始时，自身消耗50点算力以触发
//     算力乱流，然后若对方与自身均拥有算力限制中的同一个特性，则额外触发算力共振
//     回合结束时，20%解除自身所处的异常状态、20%令自身所有技能PP值+1、20%令对手所有
//     技能PP值-1、20%使自身全属性+1、20%恢复200点体力并附加等量固定伤害，
//     若所有项目均未随机到，则获得20点算力」
//   解析帖等效规则：
//   - 乱流 = 登场后 2 回合内**自身特性必定触发**（概率提升至100%）+ 五条 20% 必定触发
//     （100 算力 = 2×50；第 3 回合开始付不起 → 乱流结束）；
//   - 概率提升**仅限有概率掷点的特性**（坚硬/强袭/精神等无掷点特性不受影响；乱舞已废除
//     no-op）——引擎落点 = `trait_state_[owner].own.proc_forced`（trait_proc_roll 唯一掷点；
//     主动毒照走 Ancient、被动毒照走 Raw，**不改通道**——这是与妙时复制毒"改现代施加"
//     的本质区别，亮节"以实际概率为准"的交互因此自动正确）；
//   - 五条 20% **独立结算**；全未中 → +20 算力；
//   - 共振：对方**全队 6 只**（含场下）任一与自身同特性（解析帖：仅限瞬杀/顽强/回神）
//     → 本次在场触发。
//
// ★ 真实提高概率的落点（本线核心口径，用户 2026-09-24）：只抬概率、不动通道。
//   proc_forced 在 trait_proc_roll 里必发且不消耗 rand（100% 档同款契约）。
//   ⚠️ 虚无（VoidDodge）已在该掷点豁免必发（必修6 ③：它是命中率修正、不是概率触发，
//   否则乱流会把闪避推成 100%）——见 trait_state.h trait_proc_roll 注释。
//
// 状态全部存 **on_stage_storage**（本次上场、下场即清）——算力/限制/乱流都是"登场获得"
// 的本次上场资源，重新登场由 arm 节点重发 100（拍板 §三.5 默认：重置不累加）：
//   key1 = 算力（int）  key2 = 算力限制（bool）  key3 = 乱流激活（本回合，bool）
// 算力限制的引擎内消费者是渎神 2322（"处于算力限制则本次攻击无法造成伤害"，
// 已落地：moves_lib/jiyuan_ds001_skills.cpp 读本文件写入的 key2）；
// 共振的另一半"最大体力10%临时体力上限"未做（临时上限机制不存在，拍板 §三.10 默认半截）。
#include "common.h"

namespace {

// ⚠️ 注册键 = effect_icon.effect_id（装配层 PetFactory 以它作 SoulMark.id 查程序表）。
//    4797 是 monster id，只用于文件命名与注释——用它注册程序永远挂不上（2026-09-27 修复）。
constexpr int kJiyuanMarkId = 2414;
constexpr int kDispelChanceSite = 0;
constexpr int kRestorePpChanceSite = 1;
constexpr int kReducePpChanceSite = 2;
constexpr int kStatBoostChanceSite = 3;
constexpr int kHealAndDamageChanceSite = 4;
constexpr int kSuanliPerEntry = 100;   // 登场获得算力
constexpr int kTurbulenceCost = 50;    // 每回合开始触发乱流的算力消耗
constexpr int kNoRollBonus = 20;       // 五条全未随机到 → +20 算力
constexpr int kFiveRollPct = 20;       // 五条各自概率
constexpr int kHealAmount = 200;       // 第⑤条恢复量（"等量固定伤害"同值）

constexpr int kSuanliKey = kJiyuanMarkId * 100 + 1;
constexpr int kRestrictedKey = kJiyuanMarkId * 100 + 2;
constexpr int kTurbulenceKey = kJiyuanMarkId * 100 + 3;

// 本魂印宿主（存活 + 在场；桶节点 STAGE scope 本就只在上场时注册，这里双保险）。
ElfPet* jiyuan_pet(BattleContext* ctx, int owner) {
    if (!ctx || owner < 0 || owner > 1) {
        return nullptr;
    }
    const int slot = ctx->find_pet_with_soulmark(owner, kJiyuanMarkId);
    if (slot < 0 || slot != ctx->on_stage[owner]) {
        return nullptr;
    }
    ElfPet& pet = ctx->seerRobot[owner].elfPets[slot];
    if (pet.hp <= 0) {
        return nullptr;
    }
    return &pet;
}

int suanli_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kSuanliKey);
    return it == pet.on_stage_storage.end() ? 0 : any_int_or(it->second);
}

bool turbulence_of(const ElfPet& pet) {
    auto it = pet.on_stage_storage.find(kTurbulenceKey);
    return it != pet.on_stage_storage.end()
        && std::any_cast<bool>(&it->second) && *std::any_cast<bool>(&it->second);
}

// 算力共振判定（解析帖）：自身特性 ∈ {瞬杀, 顽强, 回神} 且对方全队 6 只（含场下）
// 任一同特性。消逝不计（is_vanished）；阵亡未消逝照算——"对方所有精灵"字面口径。
bool resonance_active(BattleContext* ctx, int owner) {
    const TraitKind self_kind = ctx->trait_state_[owner].own.kind;
    if (self_kind != TraitKind::InstantKill && self_kind != TraitKind::Tenacious
        && self_kind != TraitKind::Revival) {
        return false;
    }
    const int foe = 1 - owner;
    for (int slot = 0; slot < 6; ++slot) {
        if (ctx->is_vanished(foe, slot)) {
            continue;
        }
        const CommonTrait& trait = ctx->seerRobot[foe].elfPets[slot].commonTrait;
        if (trait_kind_from_name(trait.name) == self_kind) {
            return true;
        }
    }
    return false;
}

// 登场（战斗开始激活 + 每次上场；early 节点在激活时立即执行、scope 门限在场）：
// 算力 = 100、获得算力限制、共振则当场解除限制、乱流待下一回合开始再付。
EffectResult soulmark_4797_arm_jiyuan(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = jiyuan_pet(ctx, owner);
    if (!self) {
        return EffectResult::kOk;
    }
    self->on_stage_storage[kSuanliKey] = kSuanliPerEntry;
    self->on_stage_storage[kRestrictedKey] = !resonance_active(ctx, owner);
    self->on_stage_storage[kTurbulenceKey] = false;
    // own 槽是本场登场的特性视图（fresh copy，proc_forced 恒 false）——显式归位，
    // 防"上一次在场的乱流标志"经任何路径残留。
    ctx->trait_state_[owner].own.proc_forced = false;
    return EffectResult::kOk;
}

// 每回合开始：付算力触发乱流。付得起（≥50）→ 乱流激活 + own 特性必发；付不起 → 关。
// own 槽在 perform_switch 会整体重同步（fresh copy），不会把必发漏给后上场精灵。
EffectResult soulmark_4797_round_pay_jiyuan(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = jiyuan_pet(ctx, owner);
    if (!self) {
        return EffectResult::kOk;
    }
    int suanli = suanli_of(*self);
    const bool turbulence = suanli >= kTurbulenceCost;
    if (turbulence) {
        suanli -= kTurbulenceCost;
    }
    self->on_stage_storage[kSuanliKey] = suanli;
    self->on_stage_storage[kTurbulenceKey] = turbulence;
    ctx->trait_state_[owner].own.proc_forced = turbulence;
    return EffectResult::kOk;
}

// 回合结束五条（乱流期全发不掷点；非乱流期各独立掷 20%——解析帖"独立结算"）。
// rand 契约：乱流期一条不掷（必发不消耗，同 trait_proc_roll 100% 档）；
// 非乱流期按官方书写序逐条掷、未到的后续条目照常掷。
EffectResult soulmark_4797_round_end_jiyuan(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    const int owner = resolve_owner_from_args(ctx, args);
    ElfPet* self = jiyuan_pet(ctx, owner);
    if (!self) {
        return EffectResult::kOk;
    }
    const bool turbulence = turbulence_of(*self);
    const auto may = [ctx, owner, turbulence](int subsite, int target) {
        return turbulence || roll_soul_percent(*ctx, kJiyuanMarkId, subsite, owner, target, kFiveRollPct);
    };
    const int foe = 1 - owner;
    int fired = 0;

    // ① 20% 解除自身所处的异常状态（效果解除路径，不走自然到期/衍化）
    if (may(kDispelChanceSite, owner)) {
        if (g_core && g_core->dispel_active_anomalies) {
            (void)g_core->dispel_active_anomalies(ctx, owner);
        }
        ++fired;
    }
    // ② 20% 令自身所有技能 PP+1（clamp ≤ maxPP；无限槽不参与）
    if (may(kRestorePpChanceSite, owner)) {
        if (g_core && g_core->pp_restore) {
            (void)g_core->pp_restore(ctx, owner, 1, /*only_empty=*/false);
        }
        ++fired;
    }
    // ③ 20% 令对手所有技能 PP-1（走原语：逐槽 emit EVENT_PP_REDUCED）
    if (may(kReducePpChanceSite, foe)) {
        if (g_core && g_core->pp_reduce) {
            (void)g_core->pp_reduce(ctx, foe, 1);
        }
        ++fired;
    }
    // ④ 20% 使自身全属性+1（6 项含命中，2026-09-18 口径；走 stat_change 真原语）
    if (may(kStatBoostChanceSite, owner)) {
        if (g_core && g_core->stat_change) {
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                (void)g_core->stat_change(ctx, owner, i, 1);
            }
        }
        ++fired;
    }
    // ⑤ 20% 恢复200点体力并附加等量固定伤害（数据库口径：固定伤害；4399 作百分比，不用）。
    //     "等量"按名义 200 算，不随封回血打折（拍板前保守口径，见开工文档 §三.9）。
    if (may(kHealAndDamageChanceSite, foe)) {
        if (g_core && g_core->heal_amount) {
            (void)g_core->heal_amount(ctx, owner, kHealAmount);
        }
        if (g_core && g_core->deal_pink_damage) {
            (void)g_core->deal_pink_damage(ctx, foe, kHealAmount, DamageKind::FIXED, owner);
        }
        ++fired;
    }
    // 全未随机到 → +20 算力
    if (fired == 0) {
        self->on_stage_storage[kSuanliKey] = suanli_of(*self) + kNoRollBonus;
    }
    self->on_stage_storage[kTurbulenceKey] = false;
    return EffectResult::kOk;
}

// ── 效果注册（静态自注册：dlopen 时进本 dylib 的表）──
//  arm 用 early=true：激活时立即执行（战斗开始 + **每次上场**重发算力，官方"登场时"）。
SOUL_PROGRAM(kJiyuanMarkId, {
    SoulMarkNodeRef(State::OPERATION_ENTER_EXIT_STAGE, &soulmark_4797_arm_jiyuan,
                    /*once=*/false, /*early=*/true),
    SoulMarkNodeRef(State::BATTLE_ROUND_START, &soulmark_4797_round_pay_jiyuan,
                    /*once=*/false, /*early=*/false),
    SoulMarkNodeRef(State::BATTLE_ROUND_END, &soulmark_4797_round_end_jiyuan,
                    /*once=*/false, /*early=*/false)});

}  // namespace
