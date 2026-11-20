// D 组批产 · D22（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   斯布林蒂 4600 第五技 彼空·蔚莱唤歌 37094 缺失效果三支：
//     2110 若己方场下处于未来链接的精灵中存在精灵系别与对手相同，则该系别的精灵
//          每有1级未来链接克制倍数提升0.5（0 参）
//     1740 自身每有1项非第五技能PP值不满附加{0}点固定伤害，对手免疫固定伤害时
//          额外附加200点真实伤害（切片 [150]）
//     1821 未击败对手则恢复自身所有技能{0}点PP值（切片 [5]）
//   同技能 761（对手回血效果减少，batch_b）已有实现，不重复。
//
// 实现要点：
//   · 2110：遍历己方背包槽（非在场）future_link_level>0 的精灵，系别与对手当前
//     系别视图有交集 → bonus += 0.5×其链接等级；bonus>0 时写
//     ws.restraint_view[self] = 自然克制(技能元素视图 vs 守方系别视图) + bonus
//     （restraint_view>=0 即被伤害公式采用；未触发则保持 -1 不动）。
//     层数读共享头 future_link.h（sibulindi 魂印侧建立的每精灵状态）。
//   · 1740：统计自身槽 0..3（非第五技能）pp<maxPP 的项数 n → deal_pink_damage
//     (FIXED, 150×n)；以目标血差判"免疫固定伤害"（差 0 = 被免/拦）→ 追加
//     deal_true_damage(200)（真伤独立通道，天然不被免粉拦）。
//   · 1821：AFTER_ACTION 桥（对手存活=未击败）→ pp_restore(self, 5, false)
//     （全技能 +5，clamp 到 maxPP；本技使用扣减发生在效果之后，回合末语义成立）。

#include "common.h"
#include <effects/future_link.h>

#include <memory>
#include <vector>

namespace {

void register_after_action_bridge(BattleContext* ctx, int owner, int effect_id,
                                  std::vector<int> ints, EffectFn fn) {
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(effect_id, 0, owner, 1, EffectArgs(std::move(ints)), fn);
        auto ce = std::make_unique<ContinuousEffect>(e, st, owner, 1, ctx->roundCount);
        ce->source_id_ = effect_id;
        ctx->register_skill_effect(st, owner, std::move(ce));
    }
}

// ── 2110 未来链接系别共振 → 克制倍数提升 ────────────────────────────────────────
EffectResult effect_2110_future_link_restraint(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 1 || !g_core
        || !g_core->restraint_multiplier) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    if (self < 0 || self > 1 || opp < 0 || opp > 1) {
        return EffectResult::kOk;
    }
    const int foe_elem[2] = {ctx->ws.view_elementalAttributes[opp][0],
                             ctx->ws.view_elementalAttributes[opp][1]};
    double bonus = 0.0;
    for (int slot = 0; slot < 6; ++slot) {
        if (slot == ctx->on_stage[self]) {
            continue;   // 只看场下
        }
        const int lv = future_link_level(ctx, self, slot);
        if (lv <= 0) {
            continue;
        }
        const int pet_elem[2] = {ctx->seerRobot[self].elfPets[slot].elementalAttributes[0],
                                 ctx->seerRobot[self].elfPets[slot].elementalAttributes[1]};
        bool match = false;
        for (int a = 0; a < 2 && !match; ++a) {
            const int pa = pet_elem[a];
            if (pa == 0) {
                continue;
            }
            for (int b = 0; b < 2; ++b) {
                if (foe_elem[b] != 0 && pa == foe_elem[b]) {
                    match = true;
                    break;
                }
            }
        }
        if (match) {
            bonus += 0.5 * lv;
        }
    }
    if (bonus <= 0.0) {
        return EffectResult::kOk;   // 无共振：不写覆盖槽（保持自然克制）
    }
    const double natural = g_core->restraint_multiplier(ctx->ws.skill_element_view[self],
                                                        foe_elem);
    ctx->ws.restraint_view[self] = natural + bonus;
    return EffectResult::kOk;
}

// ── 1740 PP 不满项数 → 固定伤；免固定伤 → 追加真伤 ─────────────────────────────
EffectResult effect_1740_pp_fixed_damage(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core
        || !g_core->deal_pink_damage || !g_core->deal_true_damage) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int per = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || per <= 0) {
        return EffectResult::kOk;
    }
    // "非第五技能"= 槽 0..3（第五技槽 4 不计）
    int not_full = 0;
    for (int i = 0; i < 4; ++i) {
        const Skills& s = ctx->seerRobot[self].elfPets[ctx->on_stage[self]].skills[i];
        if (s.pp != -1 && s.pp < s.maxPP) {
            ++not_full;
        }
    }
    if (not_full <= 0) {
        return EffectResult::kOk;
    }
    const int opp_slot = ctx->on_stage[opp];
    const int hp_before = ctx->seerRobot[opp].elfPets[opp_slot].hp;
    (void)g_core->deal_pink_damage(ctx, opp, per * not_full, DamageKind::FIXED, self);
    const int dealt = hp_before - ctx->seerRobot[opp].elfPets[opp_slot].hp;
    if (dealt <= 0) {
        // 免疫固定伤害 → 额外附加 200 点真实伤害（真伤独立通道）
        (void)g_core->deal_true_damage(ctx, opp, 200, self);
    }
    return EffectResult::kOk;
}

// ── 1821 未击败 → 恢复自身所有技能 {n} 点 PP ──────────────────────────────────
EffectResult effect_1821_restore_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3 || !g_core || !g_core->pp_restore) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp <= 0) {
        return EffectResult::kOk;   // 击败了对手 → 不恢复
    }
    (void)g_core->pp_restore(ctx, self, amount, /*only_empty=*/false);
    return EffectResult::kOk;
}

EffectResult effect_1821_restore_pp_on_undeath(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int amount = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || amount <= 0) {
        return EffectResult::kOk;
    }
    register_after_action_bridge(ctx, self, 1821, {self, opp, amount},
                                 &effect_1821_restore_bridge);
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(2110, effect_2110_future_link_restraint);
SKILL_EFFECT(1740, effect_1740_pp_fixed_damage);
SKILL_EFFECT(1821, effect_1821_restore_pp_on_undeath);
