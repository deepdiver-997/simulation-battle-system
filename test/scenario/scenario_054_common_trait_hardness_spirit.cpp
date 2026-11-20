// 场景 054：通用特性（new_se stat=1）—— 坚硬 + 精神。
//
// 官方口径（赛学必修6《精灵特性》+ L254，用户 2026-09-15 拍板）：
//   · 坚硬：「受到任何伤害减少n%」——**只免红伤**；「不属于通用减伤时点，乘法计算」，
//     L254「坚硬是靠前的减伤，乘法计算，**时点早于保底伤害**」
//     → 引擎落点：新阶段 REDUCE_TRAIT（AMP_EXTRA 之后、REDUCE_FLAT 之前）。
//   · ⚠️ 坚硬数值是**官方读数错误**的唯一特性：实际 5/6/8/10/9/10%（3 星=5 星），
//     DB args 是 5/6/7/8/9/10（2/3 星错）→ 引擎用语料修正表 trait_hardness_pct，不读 args。
//     本场景 H2 故意把 args 填成**错误的 DB 值 7**，断言用的是修正表的 8 —— 防回归成"读 args"。
//   · 精神：「特殊攻击伤害增加n%」——「强袭和精神**不属于通用增伤**，乘法计算」
//     → AMP_EXTRA 阶段（乘法通道），按 ws.skill_type_view == Special 门控。
//
// 引擎结构（本轮新增，见 include/effects/trait_state.h + src/effects/common_trait.cpp）：
//   · 行为函数 = TEAM 常驻管线条目（每方一个），回调经 effective_common_trait 实时读
//     **当前登场精灵**的 commonTrait —— 切换/死亡天然正确，不需要 per-switch 重装；
//   · 运行时层 API：suppress_common_trait（扎克斯式抑制）/ copy_common_trait（天女式复制）。
//
// 验证点：
//   H1 坚硬 3 星 → 红伤 ×0.90（减伤发生在 REDUCE_TRAIT 阶段）
//   H2 ★ 坚硬 2 星：args=7（DB 错值）但实际 ×0.92（修正表 8%）—— 不许读 args
//   H3 ★ 坚硬在**通用减伤之前**：(base×0.9)×0.5 —— 与 REDUCE_PCT 的先后可观测
//   H4 ★ 坚硬**不免粉伤**：deal_pink_damage 全额落地
//   S1 精神 2 星 + 特殊系技能 → ×1.05（AMP_EXTRA 阶段）
//   S2 精神 + 物理系技能 → 不增伤（类别门控）
//   S3 抑制 API：suppress 后坚硬失效
//   S4 复制 API：copy 来的精神照常生效（copied=true）
#include <cassert>
#include <iostream>

#include <effects/effect.h>
#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kZhuangJi = 10001;   // 撞击：物理 power 35、无 side_effect
constexpr int kShuiQiang = 10013;  // 水枪：特殊 power 40、无 side_effect
constexpr int kGaoSu = 20017;      // 高速移动：属性技能（不还手）

// 探针记录（同场景 045：挂 owner 1（防守方）桶、只记"0 打 1"那一趟）。
int g_order[DamagePipeline::kPhaseCount];
int g_at[DamagePipeline::kPhaseCount];
int g_seen = 0;

CommonTrait hardness_trait(int star) {
    // args 故意填官方 DB 值（2星=7），断言却按语料修正表——见文件头 H2。
    static constexpr int kDbArgs[6] = {5, 6, 7, 8, 9, 10};
    return scenario::make_trait("坚硬", star, {kDbArgs[star], 0});
}

scenario::ScenarioBattle make_battle(const CommonTrait& attacker_trait,
                                     const CommonTrait& defender_trait,
                                     int first_skill = kZhuangJi) {
    auto p0 = scenario::make_pet(0, "攻方", {first_skill, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/300, /*atk=*/100, /*hp=*/3000,
                                 /*soul_mark=*/{}, attacker_trait);
    auto p1 = scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                 /*speed=*/100, /*atk=*/100, /*hp=*/3000,
                                 /*soul_mark=*/{}, defender_trait);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    return scenario::ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void register_trace(BattleContext& ctx) {
    g_seen = 0;
    for (DamagePhase phase : DamagePipeline::kOrder) {
        const int idx = static_cast<int>(phase);
        ctx.register_damage_effect(phase, /*owner=*/1, DamageEffectCategory::MITIGATE,
                                   [idx](BattleContext* c, int) {
                                       if (c->resolvedDamage.attackerId != 0
                                           || c->resolvedDamage.defenderId != 1) {
                                           return;
                                       }
                                       g_order[g_seen] = idx;
                                       g_at[idx] = c->resolvedDamage.final;
                                       ++g_seen;
                                   });
    }
}

void strike(scenario::ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.drive();
}

int phase_idx(DamagePhase p) { return static_cast<int>(p); }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== H1 坚硬 3 星 → 红伤 ×0.90 ==========
    {
        scenario::ScenarioBattle b = make_battle(CommonTrait{}, hardness_trait(3));
        b.start();
        register_trace(b.ctx());
        strike(b);
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        const int at_trait = g_at[phase_idx(DamagePhase::REDUCE_TRAIT)];
        const int at_flat = g_at[phase_idx(DamagePhase::REDUCE_FLAT)];
        CHECK(at_extra > 0, "H1) 减伤前伤害 > 0");
        CHECK_EQ(at_trait, at_extra * 90 / 100, "H1) 坚硬 3 星 = ×0.90（乘法）");
        CHECK_EQ(at_flat, at_trait, "H1) 之后没有其它减伤（REDUCE_FLAT 原值通过）");
        const auto t = b.ctx().effective_common_trait(1, TraitKind::Hardness);
        CHECK(t.has_value(), "H1) 查询 API：守方生效特性存在");
        CHECK_EQ(t->star_level, 3, "H1) 查询 API：星级正确");
        std::cout << "  [H1] 坚硬3星: " << at_extra << " → " << at_trait << "（×0.90）\n";
    }

    //========== H2 ★ 2 星：不读 args（7），用修正表 8% ==========
    {
        scenario::ScenarioBattle b = make_battle(CommonTrait{}, hardness_trait(2));
        b.start();
        register_trace(b.ctx());
        strike(b);
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        const int at_trait = g_at[phase_idx(DamagePhase::REDUCE_TRAIT)];
        CHECK_EQ(at_trait, at_extra * 92 / 100,
                 "H2) ★ 坚硬 2 星实际 8%（语料修正表）；若读 args 会是 ×0.93");
        CHECK(at_trait != at_extra * 93 / 100, "H2) 确实不是 DB args 的 7%");
        std::cout << "  [H2] 坚硬2星(args=7): " << at_extra << " → " << at_trait
                  << "（×0.92 = 修正表 8%，非 args 的 7%）\n";
    }

    //========== H3 ★ 坚硬在通用减伤之前 ==========
    {
        scenario::ScenarioBattle b = make_battle(CommonTrait{}, hardness_trait(3));
        b.start();
        register_trace(b.ctx());
        // 通用减伤 50%（走 SKILL_EFFECT 时点写 ws，同场景 045 套路）
        Effect e(999054, 0, 0, 1, EffectArgs(std::vector<int>{0, 1}),
                 [](BattleContext* c, const EffectArgs&) {
                     c->ws.damage_reduce_add[1][0] = 50;
                     return EffectResult::kOk;
                 });
        auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_SKILL_EFFECT, 0,
                                                     /*duration=*/1, b.ctx().roundCount);
        ce->source_id_ = 999054;
        b.ctx().register_skill_effect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce));
        strike(b);
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        const int at_trait = g_at[phase_idx(DamagePhase::REDUCE_TRAIT)];
        const int at_pct = g_at[phase_idx(DamagePhase::REDUCE_PCT)];
        CHECK_EQ(at_trait, at_extra * 90 / 100, "H3) 坚硬先乘 ×0.90");
        CHECK_EQ(at_pct, at_extra * 90 / 100 * 50 / 100,
                 "H3) ★ 通用减伤作用于坚硬**之后**的值：(base×0.9)×0.5");
        std::cout << "  [H3] 顺序: " << at_extra << " → 坚硬 " << at_trait
                  << " → 通用减伤 " << at_pct << "\n";
    }

    //========== H4 ★ 坚硬不免粉伤 ==========
    {
        scenario::ScenarioBattle b = make_battle(CommonTrait{}, hardness_trait(5));
        b.start();
        BattleContext& ctx = b.ctx();
        const int before = ctx.getPet(1).hp;
        deal_pink_damage(&ctx, 1, 100, DamageKind::FIXED, 0);
        CHECK_EQ(ctx.getPet(1).hp, before - 100,
                 "H4) ★ 粉伤全额落地（坚硬只免红伤，10% 也没减）");
        std::cout << "  [H4] 坚硬5星 vs 粉伤100: " << before << " → " << ctx.getPet(1).hp
                  << "（全额，不减）\n";
    }

    //========== S1 精神 2 星 + 特殊系 → ×1.05 ==========
    {
        scenario::ScenarioBattle b = make_battle(
            scenario::make_trait("精神", 2, {2, 5}), CommonTrait{}, kShuiQiang);
        b.start();
        register_trace(b.ctx());
        strike(b);
        const int at_amp = g_at[phase_idx(DamagePhase::AMP)];
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        CHECK(at_amp > 0, "S1) 特殊系裸伤 > 0");
        CHECK_EQ(at_extra, at_amp * 105 / 100, "S1) 精神 2 星：特攻伤害 ×1.05（乘法通道）");
        std::cout << "  [S1] 精神(特攻): " << at_amp << " → " << at_extra << "（×1.05）\n";
    }

    //========== S2 精神 + 物理系 → 不增伤 ==========
    {
        scenario::ScenarioBattle b = make_battle(
            scenario::make_trait("精神", 2, {2, 5}), CommonTrait{}, kZhuangJi);
        b.start();
        register_trace(b.ctx());
        strike(b);
        const int at_amp = g_at[phase_idx(DamagePhase::AMP)];
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        CHECK_EQ(at_extra, at_amp, "S2) 物理系技能吃不到精神（skill_type_view 门控）");
        std::cout << "  [S2] 精神(物理): " << at_amp << " → " << at_extra << "（不变）\n";
    }

    //========== S3 抑制 API：suppress 后坚硬失效 ==========
    {
        scenario::ScenarioBattle b = make_battle(CommonTrait{}, hardness_trait(3));
        b.start();
        register_trace(b.ctx());
        b.ctx().suppress_common_trait(1, TraitKind::Hardness, /*source_owner=*/0);
        strike(b);
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        const int at_trait = g_at[phase_idx(DamagePhase::REDUCE_TRAIT)];
        CHECK_EQ(at_trait, at_extra, "S3) 被抑制后坚硬不生效（扎克斯式）");
        CHECK(b.ctx().effective_common_trait(1, TraitKind::Hardness) == std::nullopt,
              "S3) 查询 API：被抑制 → nullopt");
        std::cout << "  [S3] 抑制后: " << at_extra << " → " << at_trait << "（不变）\n";
    }

    //========== S4 复制 API：copy 来的精神生效 ==========
    {
        // 攻方没有本体特性；手工构造一条 EffectiveTrait 复制上去（天女式的最小形态）。
        scenario::ScenarioBattle b = make_battle(CommonTrait{}, CommonTrait{}, kShuiQiang);
        b.start();
        register_trace(b.ctx());
        EffectiveTrait copied;
        copied.kind = TraitKind::Spirit;
        copied.idx = 1453;
        copied.star_level = 2;
        copied.args[0] = 2;
        copied.args[1] = 5;
        b.ctx().copy_common_trait(/*dest_side=*/0, copied, /*source_owner=*/1);
        strike(b);
        const int at_amp = g_at[phase_idx(DamagePhase::AMP)];
        const int at_extra = g_at[phase_idx(DamagePhase::AMP_EXTRA)];
        CHECK_EQ(at_extra, at_amp * 105 / 100, "S4) 复制来的精神照常生效");
        const auto q = b.ctx().effective_common_trait(0, TraitKind::Spirit);
        CHECK(q.has_value() && q->copied, "S4) 查询 API：copied=true");
        std::cout << "  [S4] 复制精神: " << at_amp << " → " << at_extra << "（×1.05）\n";
    }

    std::cout << "\n场景 054 通过：坚硬（REDUCE_TRAIT 阶段·乘法·只免红伤·语料修正表 "
                 "5/6/8/10/9/10）+ 精神（AMP_EXTRA·乘法·特攻门控）+ 运行时层抑制/复制 API。\n";
    return 0;
}
