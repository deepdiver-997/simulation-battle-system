// 场景 7760 —— 通用增伤（AMP·加法通道）无上限钳制（S15 百科清缴·01 章 §五#1）。
//
// 主张（01-伤害管线与时点链.md §五#1「通用增伤是否也无上限」）：
//   引擎 AMP 阶段读 ws.damage_add_pct[attacker] 后直接 checked_pct_mul（×(100+pct)/100），
//   **没有任何上限钳制**——与 REDUCE_PCT 加算槽"钳 ±100%"形成对照（钳法只存在于减伤区）。
//   唯一的保护是 int 溢出响亮 throw（calculation.h:18-32，场景堆超大 HP 才触得到）。
//
// 源引：
//   · src/fsm/battleContext.cpp install_default_damage_amp——damage.final =
//     checked_pct_mul(damage.final, pct, ...) + flat，仅负值归零，无上限钳；
//   · include/numerical-calculation/calculation.h:18-32 checked_pct_mul——
//     中间积超 int 即 throw，其余直通；
//   · 对照：Calculation::applyDamageReduction（calculation.h:97-121）加算槽钳 ±100。
//
// 断言口径（数值全用**关系式精确断言**，浮动带 [0.85,1.0] 被 GUARD_DETECT 探针值吸收）：
//   A1 damage_add_pct=250 → AMP 阶段值 == base×(100+250)/100 == base×3.5（精确），
//      且 FINAL_CORRECT 出口同值、守方掉血同值（管线后无其他修正）；
//   A2 damage_add_pct=10000 → == base×101（无钳制直通；×101 远超"减伤区 100% 上限"直觉）。
//   探针形制同场景 045：挂 owner 1（防守方）桶、只记 attackerId==0 那一趟。
//
// 全部 submit_operation + drive() 有限推进，绝不 while(!FINISHED)。
#include <iostream>
#include <vector>

#include <effects/effect.h>
#include <entities/pet_factory.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;  // 撞击：物理 power 35、无 side_effect
constexpr int kGaoSu = 20017;     // 高速移动：属性技能（不还手）

// 探针记录（同场景 045：挂 owner 1（防守方）桶、只记"0 打 1"那一趟——
// 防守方的观测点在该阶段收尾，挂 owner 0 会把观测整体前移一格）。
int g_at[DamagePipeline::kPhaseCount];
int g_seen = 0;

scenario::ScenarioBattle make_battle() {
    auto p0 = make_pet(0, "攻方", {kZhuangJi, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                       /*speed=*/300, /*atk=*/100, /*hp=*/3000);
    auto p1 = make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                       /*speed=*/100, /*atk=*/100, /*hp=*/3000);
    p0.skills[0].accuracy = 100;
    p0.skills[0].must_hit = true;
    return ScenarioBattle(make_party(p0), make_party(p1));
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
                                       g_at[idx] = c->resolvedDamage.final;
                                       ++g_seen;
                                   });
    }
}

// ws 注入走真实路径（SKILL_EFFECT 时点；ROUND_START 的 ws.reset() 会冲掉直接写）。
// EffectFn 是裸函数指针 → 待写值放全局（场景 045 同款）。
int g_amp_pct = 0;

EffectResult apply_ws_setup(BattleContext* ctx, const EffectArgs&) {
    if (!ctx) {
        return EffectResult::kOk;
    }
    ctx->ws.damage_add_pct[0] = static_cast<float>(g_amp_pct);
    return EffectResult::kOk;
}

void register_ws_setup(BattleContext& ctx) {
    Effect e(999077, /*priority=*/0, /*owner=*/0, /*left_round=*/1,
             EffectArgs(std::vector<int>{0, 1}), &apply_ws_setup);
    auto ce = std::make_unique<ContinuousEffect>(e, State::BATTLE_FIRST_SKILL_EFFECT, 0,
                                                 /*duration=*/1, ctx.roundCount);
    ce->source_id_ = 999077;
    ctx.register_skill_effect(State::BATTLE_FIRST_SKILL_EFFECT, 0, std::move(ce));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== A1 通用增伤 +250% → 精确 ×3.5，无上限钳制 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        register_trace(ctx);
        register_ws_setup(ctx);
        g_amp_pct = 250;

        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        const int base = g_at[static_cast<int>(DamagePhase::GUARD_DETECT)];
        CHECK(base > 0, "A1 链首快照应为正的裸伤（浮动带已落在 base）");
        // checked_pct_mul(base, 250) = base*(100+250)/100 = base*350/100（int 整除，同式复算）。
        const long long expected = static_cast<long long>(base) * 350 / 100;
        CHECK_EQ(g_at[static_cast<int>(DamagePhase::AMP)], expected,
                 "A1 AMP 阶段值应=base×3.5（+250% 无钳直通）");
        CHECK_EQ(g_at[static_cast<int>(DamagePhase::FINAL_CORRECT)], expected,
                 "A1 管线出口应同值（无其他阶段介入）");
        CHECK_EQ(battle.pet(1).hp, 3000 - expected,
                 "A1 守方掉血应=管线出口值（无护盾/护罩吸收）");
        std::cout << "  [A1] +250% 精确 ×3.5（base=" << base << " → " << expected
                  << "），无钳制 通过" << std::endl;
    }

    //========== A2 通用增伤 +10000% → 精确 ×101，仍无钳制 ==========
    {
        auto battle = make_battle();
        BattleContext& ctx = battle.ctx();
        register_trace(ctx);
        register_ws_setup(ctx);
        g_amp_pct = 10000;

        battle.start();
        battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        battle.drive();

        const int base = g_at[static_cast<int>(DamagePhase::GUARD_DETECT)];
        CHECK(base > 0, "A2 链首快照应为正");
        // base*(100+10000)/100 = base*101；中间积 ~30×10100 ≈ 3e5 远离 int 溢出。
        const long long expected = static_cast<long long>(base) * 10100 / 100;
        CHECK_EQ(g_at[static_cast<int>(DamagePhase::AMP)], expected,
                 "A2 AMP 阶段值应=base×101（+10000% 仍直通）");
        CHECK_EQ(g_at[static_cast<int>(DamagePhase::FINAL_CORRECT)], expected,
                 "A2 管线出口同值");
        std::cout << "  [A2] +10000% 精确 ×101（base=" << base << " → " << expected
                  << "），仍无钳制 通过" << std::endl;
    }

    std::cout << "\n场景 7760 通过：通用增伤（AMP·加法）无上限钳制——"
                 "减伤区的 ±100% 钳法不存在于增伤区，唯一保护=int 溢出响亮报错"
              << std::endl;
    return 0;
}
