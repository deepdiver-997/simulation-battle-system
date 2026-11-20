// 场景 1880 —— J98 切换回血族（1 宏，moves_lib/j_switch_heal.cpp）
//（2026-10-03，worktree ../agents/j98；号段 1880-1889 → T1880 一块）
//
// 断言口径：
//   - 1702（牵风引霞 [1779✗,1702]|[1,3,300]——1779 哑）：R1 我方 CHOOSE_PET 主动
//     中切（-slot1）→ SWAP 武装 + ENTER_STAGE 回血 → 下只精灵 3000→3300；
//     对照 = 无切换（场下精灵不受影响）；
//   - 场下宠血量经 seerRobot[0].elfPets[slot] 直改（overclock 同路径）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;  // 拍打：物40
constexpr int kSongKe = 36703; // 牵风引霞 [1779,1702]|[1,3,300]（切换回血载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1880 1702 主动切换 → 下只出战回 300（牵风引霞 [300]）══
    {
        // a) R1 主动中切（slot0 → slot1）：下只出战回 300
        {
            ScenarioBattle b(
                make_party(mk_pet({kSongKe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[1].hp = 3000;   // 场下宠预置缺口
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放载体（挂 watcher）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);   // R2 主动中切
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.seerRobot[0].elfPets[1].hp, 3300,
                     "T1880.a 1702：主动切换 → 下只出战回 300（实测 " +
                         std::to_string(ctx.seerRobot[0].elfPets[1].hp) + "）");
            CHECK_EQ(ctx.on_stage[0], 1, "T1880.a 前置：slot1 已登场");
        }
        // b) 对照：无切换 → 场下宠不变
        {
            ScenarioBattle b(
                make_party(mk_pet({kSongKe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[1].hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.seerRobot[0].elfPets[1].hp, 3000,
                     "T1880.b 1702 对照：无切换不回（实测 " +
                         std::to_string(ctx.seerRobot[0].elfPets[1].hp) + "）");
        }
        std::cout << "  [T1880] 1702 切换回血 通过\n";
    }

    std::cout << "scenario_1880 PASS (J98 切换回血族 1702)" << std::endl;
    return 0;
}
