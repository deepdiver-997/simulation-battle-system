// 绮影罗心·织焰 4696 魂印 —— 烧伤换血
//
// 官方魂印（effect_icon 2300）：
//   对手使用技能后，若双方任意一方处于烧伤状态则与对手交换体力值百分比；
//   回合结束后，若双方任意一方不处于烧伤状态，则自身恢复已损失体力值的30%（boss无效）。
//
// 关键口径（2026-09-20 烧伤三精灵线）：
//   ★ "双方任意一方处于烧伤状态" = **有效烧伤**（真实 ∪ 虚拟）——上古炎兽把免弱对手
//     "视为烧伤"后，织焰对其换血照样成立（官方交互：换血可以直接换死很多精灵）。
//   ★ 换血 = **百分比交换**：各自按自己的最大体力换算对方的体力百分比（773 是绝对值
//     互换，本条不是）。换到 0 血走 defeat_pet 漏斗（EFFECT 归因），死亡拦截器/免死/
//     击败监听全链路照常。
//   ★ "对手使用技能后"：先/后手桶各判一次"本桶行动方是否对手且真点了技能"
//     （preemptive_right 定先手方，与 FSM resolve_first_mover_id 同源；嗑药/被控不算）。
//   ★ "不处于烧伤状态" = 并非双方都在烧：只要有一侧没烧，回合结束就回已损失的 30%
//     （走 heal 原语：吃封回血——包括希拓烧伤锁疗的封锁）。
//   ★ (boss无效)：引擎暂无 boss 概念，不设例外。
#include "common.h"
#include <effects/burn_perception.h>
#include "../burn_trio_runtime.h"

namespace {

// "对手使用技能后"已响应的回合号（on_stage_storage）——双节点去重用。
// ⚠️ 桶语义：AFTER_ACTION 族只跑**行动方**自己的条目（execute_registered_actions(mover)），
// 本方节点抓不到"对方桶"。可行捕获点只有两个：
//   · 本方为**后手** → 本方 BATTLE_SECOND_ACTION_START（先手方已行动完，最早的响应点）；
//   · 本方为**先手** → 本方 BATTLE_ROUND_END 桶（ROUND_END 双侧都跑；对方行动后本方
//     没有更早的自有桶）。两节点以"已响应回合号"去重，一回合只换一次。
constexpr int kZhiyanReactedRoundKey = 2301;

bool zhiyan_on_stage(BattleContext* ctx, int side) {
    return ctx && side >= 0 && side <= 1 && ctx->on_stage[side] >= 0
        && ctx->on_stage[side] < 6 && ctx->getPet(side).id == kZhiyanPetId;
}

int first_mover_of(const BattleContext* ctx) {
    return ctx->preemptive_right == PreemptiveRight::SEER_ROBOT_2 ? 1 : 0;
}

bool opponent_used_skill(const BattleContext* ctx, int side) {
    const int opp = 1 - side;
    return ctx->roundChoice[opp][0] == 1 /*SELECT_SKILL*/;
}

// ── 换血本体：双方任意一方处于（有效）烧伤 → 交换体力值百分比 ──
void zhiyan_swap_hp(BattleContext* ctx, int side) {
    const int opp = 1 - side;
    if (!is_burn_effective(ctx, side) && !is_burn_effective(ctx, opp)) {
        return;
    }
    ElfPet& self = ctx->getPet(side);
    ElfPet& foe = ctx->getPet(opp);
    const long long self_max = std::max(1, self.numericalBase[NumericalPropertyIndex::HP]);
    const long long foe_max = std::max(1, foe.numericalBase[NumericalPropertyIndex::HP]);
    // 百分比互换：各自以对方当前体力百分比 × 自己最大体力重算（64 位中间量防溢出）。
    const long long new_self = static_cast<long long>(foe.hp) * self_max / foe_max;
    const long long new_foe = static_cast<long long>(self.hp) * foe_max / self_max;
    self.hp = static_cast<int>(std::min<long long>(std::max<long long>(new_self, 0), self_max));
    foe.hp = static_cast<int>(std::min<long long>(std::max<long long>(new_foe, 0), foe_max));
    // 换到 0 血 → 走死亡漏斗（效果直接扣血归因；死亡拦截器/免死/击败监听照常）。
    if (g_core && g_core->defeat_pet) {
        if (self.hp <= 0) {
            self.hp = 0;
            g_core->defeat_pet(ctx, side, ctx->on_stage[side], /*actor=*/opp,
                               DefeatCause::EFFECT);
        }
        if (foe.hp <= 0) {
            foe.hp = 0;
            g_core->defeat_pet(ctx, opp, ctx->on_stage[opp], /*actor=*/side,
                               DefeatCause::EFFECT);
        }
    }
}

// 节点①：本方为后手 —— 先手对手刚行动完（本方 BATTLE_SECOND_ACTION_START）。
EffectResult zhiyan_after_opp_as_second(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !zhiyan_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (!opponent_used_skill(ctx, side)) {
        return EffectResult::kOk;   // 对手嗑药/被控/换宠：不算"使用技能"
    }
    ElfPet& host = ctx->getPet(side);
    host.on_stage_storage[kZhiyanReactedRoundKey] = ctx->roundCount;
    zhiyan_swap_hp(ctx, side);
    return EffectResult::kOk;
}

// 节点②：本方为先手 —— 对手（后手）行动后的最早自有桶 = 本方 ROUND_END。
EffectResult zhiyan_after_opp_as_first(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !zhiyan_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (first_mover_of(ctx) != side || !opponent_used_skill(ctx, side)) {
        return EffectResult::kOk;
    }
    ElfPet& host = ctx->getPet(side);
    const auto it = host.on_stage_storage.find(kZhiyanReactedRoundKey);
    if (it != host.on_stage_storage.end()
        && any_int_or(it->second) == ctx->roundCount) {
        return EffectResult::kOk;   // 节点①已响应（本方后手路径）
    }
    host.on_stage_storage[kZhiyanReactedRoundKey] = ctx->roundCount;
    zhiyan_swap_hp(ctx, side);
    return EffectResult::kOk;
}

// ── 回合结束：并非双方都在烧 → 恢复已损失体力值的 30%（走 heal 原语，吃封回血）──
EffectResult zhiyan_round_end(BattleContext* ctx, const EffectArgs& args) {
    const int side = resolve_owner_from_args(ctx, args);
    if (side < 0 || side > 1 || !zhiyan_on_stage(ctx, side)) {
        return EffectResult::kOk;
    }
    if (is_burn_effective(ctx, 0) && is_burn_effective(ctx, 1)) {
        return EffectResult::kOk;   // 双方都在烧：不恢复
    }
    ElfPet& self = ctx->getPet(side);
    const int max_hp = resolve_pet_max_hp(self);
    const int lost = max_hp - self.hp;
    if (lost <= 0 || !g_core || !g_core->heal_amount) {
        return EffectResult::kOk;
    }
    g_core->heal_amount(ctx, side, lost * 30 / 100);
    return EffectResult::kOk;
}

}  // namespace

SOUL_PROGRAM(kZhiyanMarkId,
             {SoulMarkNodeRef(State::BATTLE_SECOND_ACTION_START, &zhiyan_after_opp_as_second,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &zhiyan_after_opp_as_first,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE),
              SoulMarkNodeRef(State::BATTLE_ROUND_END, &zhiyan_round_end,
                              /*once=*/false, /*early=*/false, SoulScope::STAGE)});
