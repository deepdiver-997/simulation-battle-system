// 场景 2560 —— J154 每回合变化族（643，moves_lib/j_roundly_change.cpp）
//（2026-10-04，worktree ../agents/j154；号段 2560-2569）
//
// 断言口径（飞虎断魂斩 16498 [100,3,1,-1] 物160：100% 确定档，3回合 防御-1/回合；
//           语义码 1=防 → 引擎槽 2）：
//   - (a) R1 首跳 -1；R2/R3 tick 各 -1（累计 -3）；R4 窗过期恒 -3。
//   - (b) 免弱魂对照：stat_drop 全 IMMUNE → 槽位恒 0。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打
constexpr int kFeiHu = 16498;   // 飞虎断魂斩 [643]|[100,3,1,-1]（物160）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "逐回合变化", skills, /*speed=*/300, /*atk=*/1, 5000);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, bool drop_immune = false) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 999999,
                        drop_immune ? make_stat_drop_immunity_soul_mark() : SoulMark{});
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    {   // (a) 逐跳累计
        ScenarioBattle b(make_party(self_pet({kFeiHu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -1, "T2560.a R1 首跳：防御 -1（引擎槽 2）");
        for (int r = 2; r <= 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][2], -r,
                     "T2560.a R" + std::to_string(r) + "：防御 -" + std::to_string(r));
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -3, "T2560.a R4：窗过期恒 -3");
        std::cout << "  [T2560.a] 逐跳累计 通过\n";
    }

    {   // (b) 免弱对照
        ScenarioBattle b(make_party(self_pet({kFeiHu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*drop_immune=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], 0, "T2560.b 免弱对照：槽位不动");
        std::cout << "  [T2560.b] 免弱对照 通过\n";
    }

    std::cout << "scenario_2560 ALL PASS" << std::endl;
    return 0;
}
