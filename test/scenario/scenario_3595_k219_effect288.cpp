#include <utils/sbs_rand.h>
// 场景 3595 —— K219 effect 288（首登场窗攻附固伤族）：邪恶魔神 2422（args[3,50]）
//
//   A 窗内 r1 攻击：对手掉血 = 红伤 2 + 粉伤 50（atk=1 精确差账）。
//   B 窗界 r3 攻击：仍附加（2+50）。
//   C 窗外 r4 攻击：只剩红伤 2。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark() {
    SoulMark sm(288, "K288", "test", EffectArgs({3, 50}));
    sm.monster_id = 2422;
    return sm;
}

// 0 方带 mark 在第 attack_round 回合攻击（其余 NONE），返回对手总掉血。
int probe_loss(int attack_round, int total_rounds) {
    ElfPet holder = scenario::make_pet(1, "邪恶魔神", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/1, /*hp=*/1000000, make_mark());
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    for (int r = 1; r <= total_rounds; ++r) {
        b.submit_operation(0, r == attack_round ? BattleFsm::ActionType::SELECT_SKILL
                                                : BattleFsm::ActionType::NONE,
                           r == attack_round ? 0 : -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 窗内 r1：红伤 2 + 附 50 ═════════════════════════════════════════
    CHECK_EQ(probe_loss(1, 1), 52, "A r1 攻击：附加 50 固伤（2+50）");

    // ══ B 窗界 r3：仍附加 ═════════════════════════════════════════════════
    CHECK_EQ(probe_loss(3, 3), 52, "B r3（窗界）仍附加");

    // ══ C 窗外 r4：只剩红伤 ═══════════════════════════════════════════════
    CHECK_EQ(probe_loss(4, 4), 2, "C r4（窗外）：无附加");

    std::cout << "scenario_3595 PASS (K219 288 首登场窗攻附固伤)" << std::endl;
    return 0;
}
