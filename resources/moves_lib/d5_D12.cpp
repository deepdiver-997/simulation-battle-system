// D 组批产 · D12（2026-09-28，night-3；工单：docs_local/docs/05-任务清单/技能组批产工单.md）
//
//   圣光莫妮卡 3711 第五技 王·再见桃花源 32519 缺失效果：
//     1152 自身不处于能力提升状态则造成的伤害提升{0}%（切片 [75]）
//     1386 当回合击败对手则回合结束时损失{0}层自然祝福（切片 [1]）
//   同技能 760（不微弱）已实现；842（处于提升→威力+50%，正条件分支）已实现——
//   1152 恰为其互补分支（count==0 才给），两支合起来与 4399 原文两半严格互斥完备。
//
// 实现：
//   · 1152：count_boost_kinds（本体 6 槽任一 >0 计提升，842 同款判据）==0 →
//     ws.damage_add_pct += {0}。文本无"额外"二字 → **通用增伤**加法通道
//     （官方 L352：通用增伤加算、非通用乘算；"额外提升"族才走 AMP_EXTRA）。
//   · 1386：AFTER_ACTION 桥（本回合一次、双桶只跑行动方那条）判"当回合击败对手"
//     （对手在场宠 hp<=0，582/1049 同判据）→ 武装 BATTLE_ROUND_END（"回合结束时"
//     官方桶，1165/1655 同注册面）一次性节点 → 扣减自然祝福 {0} 层（钳 0）。
//     自然祝福存取走共享头 monika_runtime.h（kMoniPubNz，未神谕旧资源，上限 4、
//     与宁界之律互不相通——2026-09-25 口径），不在本文件复刻存储。

#include "common.h"
#include "../monika_runtime.h"

#include <memory>
#include <vector>

namespace {

// 本体 6 槽任一提升 >0 计"处于能力提升状态"（batch_a 842 同款判据，本地一份）。
int count_boost_kinds(const BattleContext* ctx, int owner) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx->ability_levels[owner][i] > 0) {
            ++n;
        }
    }
    return n;
}

// ── 1152 自身不处于能力提升状态则造成的伤害提升{0}%；Args=[owner, target, pct] ──
EffectResult effect_1152_no_boost_amp(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int owner = args.int_args[0];
    if (owner < 0 || owner > 1) {
        return EffectResult::kOk;
    }
    if (count_boost_kinds(ctx, owner) > 0) {
        return EffectResult::kOk;   // 处于提升状态 → 本条不给（842 管正条件）
    }
    return effect_set_damage_amp(ctx, args);   // 通用增伤（加法通道）
}

// ── 1386 当回合击败对手则回合结束时损失{0}层自然祝福 ──────────────────────────
EffectResult effect_1386_round_end_loss(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 2) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int loss = args.int_args[1];
    if (self < 0 || self > 1 || loss <= 0) {
        return EffectResult::kOk;
    }
    const int nz = moni_pub_get(ctx->plugin_storage, kMoniPubNz + self);
    moni_pub_set(ctx->plugin_storage, kMoniPubNz + self, nz > loss ? nz - loss : 0);
    return EffectResult::kOk;
}

EffectResult effect_1386_kill_bridge(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int loss = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || loss <= 0) {
        return EffectResult::kOk;
    }
    if (ctx->getPet(opp).hp > 0) {
        return EffectResult::kOk;   // 当回合未击败 → 不扣
    }
    // ⚠️ "回合结束时"节点挂 **OLD_ROUND_END_1**（1679/561 击败族同款一次性节点），
    // 不挂字面更近的 BATTLE_ROUND_END：先手击杀时引擎跳过后手流程连同 BATTLE_ROUND_END
    // 整桶不跑（"skipping second mover flow and round end"），OLD_ROUND_END_1 是
    // 击杀路径也必跑的共享桶。
    Effect e(1386, 0, self, 1, EffectArgs(std::vector<int>{self, loss}),
             &effect_1386_round_end_loss);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_OLD_ROUND_END_1, self,
                                                 /*duration=*/1, ctx->roundCount);
    ce->source_id_ = 1386;
    ctx->register_skill_effect(State::BATTLE_OLD_ROUND_END_1, self, std::move(ce));
    return EffectResult::kOk;
}

EffectResult effect_1386_lose_blessing_on_kill(BattleContext* ctx, const EffectArgs& args) {
    if (!ctx || !args.int_args || args.int_count < 3) {
        return EffectResult::kOk;
    }
    const int self = args.int_args[0];
    const int opp = args.int_args[1];
    const int loss = args.int_args[2];
    if (self < 0 || self > 1 || opp < 0 || opp > 1 || loss <= 0) {
        return EffectResult::kOk;
    }
    // AFTER_ACTION 桥：FIRST/SECOND 各一条，只有行动方那条跑（582/1049 同款）。
    for (State st : {State::BATTLE_FIRST_AFTER_ACTION, State::BATTLE_SECOND_AFTER_ACTION}) {
        Effect e(1386, 0, self, 1, EffectArgs(std::vector<int>{self, opp, loss}),
                 &effect_1386_kill_bridge);
        auto ce = std::make_unique<ContinuousEffect>(e, st, self, /*duration=*/1,
                                                     ctx->roundCount);
        ce->source_id_ = 1386;
        ctx->register_skill_effect(st, self, std::move(ce));
    }
    return EffectResult::kOk;
}

}  // namespace

SKILL_EFFECT(1152, effect_1152_no_boost_amp);
SKILL_EFFECT(1386, effect_1386_lose_blessing_on_kill);
