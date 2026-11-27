#include <utils/sbs_rand.h>
// 场景 320：镇世·乔特鲁德 4186 五技缺口（1670/833/840/1709）
// （2026-09-28，night-3 / D16；实现 resources/moves_lib/d5_D16.cpp）
//
// 载体：王·浩气镇世 34900（[760,1670,833,840]/[50,7,30,3,3]）与
//       王·荒合归心 37295（[760,1187,1709,840]/[100,7,3,2]）。
// T1 1670 兜底：对手免异常（疲惫必不落地）+ 预挂回合类 → 消回合兜底触发。
// T2 833：未击败（对手存活）→ 回合结束时吸取对手最大体力 1/3（我 1000→34333）。
// T3 840：击杀 → 对方下只登场时己方在场吸取 1/3（我 50000→83333、替补 100000→66667）。
// T4 1709：未击败 → 附加 1/3 百分比伤 + 等量回血（我 1000→34333）。
// T5 1709 击败：不回血（击杀后 CHOOSE 前我方仍 50000）；840 吸取在替补登场后 83333。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;       // 拍打
constexpr int kHaoQi = 34900;     // 王·浩气镇世：[760,1670,833,840]
constexpr int kHuangHe = 37295;   // 王·荒合归心：[760,1187,1709,840]
constexpr int kFatigue = static_cast<int>(AbnormalStatusId::Fatigue);

ElfPet qiaote(int skill) {
    ElfPet p = scenario::make_pet(4186, "镇世·乔特鲁德",
                                  {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/20, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 1670 兜底：疲惫被免 → 消除对手回合类效果 ==========
    {
        ScenarioBattle b(scenario::make_party(qiaote(kHaoQi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/3, /*source_id=*/999);
        ctx.rule_center_.grant_reflect(1, -1, 999, 1, 3);
        CHECK(ctx.rule_center_.has_round_type(1), "T1 前提：对手处于回合类效果");
        one_round(b);
        CHECK(ctx.has_active_abnormal_status(1, kFatigue) == false,
              "T1 疲惫被免（未触发）");
        CHECK(ctx.rule_center_.has_round_type(1) == false, "T1 兜底：回合类被消除");
        std::cout << "  [T1] 1670 兜底消回合 通过\n";
    }

    //========== T2 833 未击败 → 回合末吸取对手最大体力 1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(qiaote(kHaoQi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 1000;   // 压低自身体力（上限 100000）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1); // 排除对手攻击干扰回血量
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 34333, "T2 回合末吸取 33333（100000/3）");
        std::cout << "  [T2] 833 未击败回合末吸取 通过（我方 hp "
                  << ctx.getPet(0).hp << "）\n";
    }

    //========== T3 840 击杀 → 对方下只登场时吸取 1/3 ==========
    {
        ScenarioBattle b(scenario::make_party(qiaote(kHaoQi)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        one_round(b);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(ctx.getPet(0).hp, 83333, "T3 己方在场吸取 33333（50000+33333）");
        CHECK_EQ(ctx.getPet(1).hp, 66667, "T3 登场宠被吸取 33333（100000→66667）");
        std::cout << "  [T3] 840 击杀→下只登场吸取 通过\n";
    }

    //========== T4 1709 未击败：附加 1/3 百分比伤 + 等量回血 ==========
    {
        ScenarioBattle b(scenario::make_party(qiaote(kHuangHe)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 1000;
        const int foe_hp_before = ctx.getPet(1).hp;
        one_round(b);
        CHECK(ctx.getPet(1).hp < foe_hp_before, "T4 附加百分比伤已落地");
        CHECK_EQ(ctx.getPet(0).hp, 34333, "T4 等量回血 33333（1000+33333）");
        std::cout << "  [T4] 1709 未击败 → 百分比伤+等量回血 通过\n";
    }

    //========== T5 1709 击败：不回血；840 吸取在替补登场后 ==========
    {
        ScenarioBattle b(scenario::make_party(qiaote(kHuangHe)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 40000;
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp, 40000, "T5 击败路径：1709 不回血（CHOOSE 前无变化）");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        CHECK_EQ(ctx.getPet(0).hp, 90000, "T5 840 吸取 1/2：替补登场后 40000+50000");
        std::cout << "  [T5] 1709 击败不回血 + 840 吸取 通过\n";
    }

    std::cout << "scenario_320 all passed" << std::endl;
    return 0;
}
