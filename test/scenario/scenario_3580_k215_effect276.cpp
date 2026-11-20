#include <utils/sbs_rand.h>
// 场景 3580 —— K215 effect 276（登场窗受击消强族）：格莱奥丹 2499（零参）
//
//   A 窗内 r1 受击：攻方 +2 攻被清零。
//   B 窗界含登场回合：r3 受击仍清。
//   C 窗外 r4 受击：不清（+2 保留）。
//   D 非攻击伤害（属性技不产生攻击伤害时点事件）→ 攻方不出手时不清；改用：
//     持有方受对攻时点之外的受击路径不做窗判定——以 A/B/C 覆盖核心窗语义。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技
constexpr int kPaiDa = 10004;   // 拍打：攻击载体
constexpr int kAtkSlot = 0;

SoulMark make_mark() {
    SoulMark sm(276, "K276", "test", EffectArgs());
    sm.monster_id = 2499;
    return sm;
}

// 持有方带 mark，攻方带 +2 攻并在第 attack_round 回合拍打（其余 NONE），
// 返回 r 末攻方攻击等级。
int probe_attacker_level(int attack_round, int total_rounds) {
    ElfPet holder = scenario::make_pet(1, "格莱奥丹", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                       make_mark());
    ElfPet foe = scenario::make_pet(2, "对手", {kPaiDa, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                    /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
    ScenarioBattle b(scenario::make_party(holder), scenario::make_party(foe));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.ability_levels[1][kAtkSlot] = 2;
    for (int r = 1; r <= total_rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, r == attack_round ? BattleFsm::ActionType::SELECT_SKILL
                                                : BattleFsm::ActionType::NONE,
                           r == attack_round ? 0 : -1);
        b.drive();
    }
    return ctx.ability_levels[1][kAtkSlot];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 窗内 r1 受击：攻方强化清零 ══════════════════════════════════════
    CHECK_EQ(probe_attacker_level(1, 1), 0, "A r1 受击 → 攻方 +2 被清");

    // ══ B 窗界含登场回合：r3 受击仍清 ═════════════════════════════════════
    CHECK_EQ(probe_attacker_level(3, 3), 0, "B r3 受击（第3回合）→ 仍清");

    // ══ C 窗外 r4 受击：不清 ══════════════════════════════════════════════
    CHECK_EQ(probe_attacker_level(4, 4), 2, "C r4 受击（窗外）→ +2 保留");

    std::cout << "scenario_3580 PASS (K215 276 登场窗受击消强)" << std::endl;
    return 0;
}
