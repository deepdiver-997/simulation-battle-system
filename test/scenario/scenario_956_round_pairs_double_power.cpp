#include <utils/sbs_rand.h>
// 场景 956 —— K4 每回合能力对 + 异常威力翻倍（effect 636 凯普斯特）
//
// args 布局 = count + (ENGINE槽, delta)×count：308 行 "2 0 1 4 1" = 攻+1 速+1；
// 440 伙伴行 "2 0 2 4 2" = 攻+2 速+2。另一子句"对手处于异常状态时自身攻击威力翻倍"
// → SKILL_USED 时写 skill_power_view（变威力管线，1443 同路）。
// 威力断言：同一 rand 档下，对手中毒 vs 不中毒 → 伤害恰 2 倍（威力线性进伤害公式）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 害怕：无跳血，只作"处于异常"判据

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 0 方带魂印攻 rounds 回合（每回合都出燕返），返回 1 方最终总损失。
int probe_damage(const SoulMark& mark, bool poison_foe, int rounds) {
    ScenarioBattle b(scenario::make_party(scenario::make_pet(
                         1, "凯普斯特", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/300, /*atk=*/5000, /*hp=*/1000000, mark)),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    b.start();
    if (poison_foe) {
        apply_anomaly(&ctx, 1, kFear, /*duration_rounds=*/9, /*actor=*/0);
    }
    sbs_srand(20260916);   // 施加之后再定档：两侧伤害掷点逐位对齐（apply 本身可能耗 rand）
    const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 回合 1：攻+1 速+1（槽 0/4）════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "凯普斯特", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(636, {2, 0, 1, 4, 1}, 2891))),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 1, "A 攻击 +1");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "A 速度 +1");
    }
    // ══ B 伙伴行 delta=2：攻+2 速+2（两回合累计 4）══════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "凯普斯特", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(636, {2, 0, 2, 4, 2}, 2891))),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        for (int r = 0; r < 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(ctx.ability_levels[0][0], 4, "B 伙伴行两回合攻击 +4");
        CHECK_EQ(ctx.ability_levels[0][4], 4, "B 伙伴行两回合速度 +4");
    }
    // ══ C 对手异常 → 威力翻倍（伤害恰 2 倍）═════════════════════════════════
    {
        const SoulMark mark = make_mark(636, {2, 0, 1, 4, 1}, 2891);
        const int plain = probe_damage(mark, false, 1);
        const int vs_abnormal = probe_damage(mark, true, 1);
        CHECK(plain > 1000, "C 基线量级");
        // 比率带：威力 ×2 是乘法关系，两局 85%~100% 浮动掷点独立 → 比率 ∈
        // [2×0.85, 2×(1/0.85)] ≈ [1.7, 2.36]。下界排掉"没翻倍"（≤1.18），上界排掉二次翻倍。
        const double ratio = static_cast<double>(vs_abnormal) / plain;
        CHECK(ratio >= 1.7 && ratio <= 2.4, "C 对手处于异常时伤害翻倍（比率带 [1.7,2.4]）");
    }

    std::cout << "scenario_956 PASS (K4 636 回合能力对 + 异常威力翻倍)" << std::endl;
    return 0;
}
