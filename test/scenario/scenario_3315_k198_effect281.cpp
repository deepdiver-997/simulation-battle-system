#include <utils/sbs_rand.h>
// 场景 3315 —— K198 effect 281（未受击回血族）：潘多拉 2518（零参）
//
//   A 受击回合：r1 对手拍打（红伤 2）→ 战阶结束不回血（hp 差恰为 2）。
//   B 未受击回合：双方 NONE → +100 精确（hp 压低不触顶）。
//   C 隔回合恢复：r1 受击不回、r2 未受击回 100。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技
constexpr int kPaiDa = 10004;   // 拍打：攻击载体（atk=1 → 红伤 2）

SoulMark make_mark() {
    SoulMark sm(281, "K281", "test", EffectArgs());
    sm.monster_id = 2518;
    return sm;
}

ElfPet make_holder() {
    return scenario::make_pet(1, "潘多拉", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/300, /*atk=*/10, /*hp=*/1000000, make_mark());
}

ElfPet make_foe(int atk) {
    return scenario::make_pet(2, "对手", {kPaiDa, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/atk, /*hp=*/1000000);
}

// 跑 one_round=rounds 回合（0 方 NONE；1 方 foe_attacks=true 时拍打），返回 0 方 hp。
int probe_hp(bool foe_attacks, int rounds) {
    ScenarioBattle b(scenario::make_party(make_holder()), scenario::make_party(make_foe(1)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 5000;   // 压低避封顶
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, foe_attacks ? BattleFsm::ActionType::SELECT_SKILL : BattleFsm::ActionType::NONE,
                           foe_attacks ? 0 : -1);
        b.drive();
    }
    return ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 受击回合不回血：5000 - 2（对手拍打）══════════════════════════════
    CHECK_EQ(probe_hp(/*foe_attacks=*/true, 1), 5000 - 2, "A 受击：无回血（恰红伤 2）");

    // ══ B 未受击回合：5000 + 100 ══════════════════════════════════════════
    CHECK_EQ(probe_hp(false, 1), 5100, "B 未受击：+100");

    // ══ C 隔回合：r1 受击 4998、r2 未受击 5098 ════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(make_holder()), scenario::make_party(make_foe(1)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 5000 - 2, "C r1 受击不回");
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, 5000 - 2 + 100,
                 "C r2 未受击 +100");
    }

    std::cout << "scenario_3315 PASS (K198 281 未受击回血)" << std::endl;
    return 0;
}
