// 场景 2720 —— J170 未暴吸取族（844，moves_lib/j_crit_miss_drain.cpp）
//（2026-10-04，worktree ../agents/j170；号段 2720-2729 → T2720 两块）
//（⚠️ 同单 1415 与 J137 重复——记档跳过不产码，见实现文件头）
//
// 断言口径：载体 斩月落云破 19903 [844]→[3]（物150，伴生 1002✓ 良性）：
//   (a) 默认暴击率 0（make_pet 清零）→ 恒未暴 → 吸对手最大体力 1/3（99999/3=33333
//       精确），自身按实际落地回血；
//   (b) set_crit_sixteenths(pet,16)=必暴 → crit_happened=true → 无吸取
//       （对手只掉红伤 final，自身不回血）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhanYue = 19903;   // 斩月落云破：物150 [844] → [3]
constexpr int kPaDa    = 10004;   // 拍打：filler

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, bool force_crit = false) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    if (force_crit) {
        set_crit_sixteenths(p, 16);   // 必暴（harness 提供）
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2720.a 未暴 → 吸最大体力 1/3 ══
    {
        ScenarioBattle b(make_party(self_pet({kZhanYue, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.crit_happened[0], "T2720.a 前置：暴击率 0 → 未暴");
        const int final = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, 99999 - final - 33333,
                 "T2720.a 844：吸对手最大体力 1/3=33333（+红伤 final）");
        CHECK_EQ(ctx.getPet(0).hp, 5000 + 33333, "T2720.a 844：按实际落地回血");
        std::cout << "  [T2720.a] 844 未暴吸取 通过\n";
    }

    // ══ T2720.b 必暴 → 无吸取 ══
    {
        ScenarioBattle b(make_party(self_pet({kZhanYue, kPaDa, kPaDa, kPaDa, kPaDa},
                                             /*force_crit=*/true)),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.crit_happened[0], "T2720.b 前置：必暴生效");
        const int final = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, 99999 - final, "T2720.b 844：暴击 → 无吸取（只掉红伤）");
        CHECK_EQ(ctx.getPet(0).hp, 5000, "T2720.b 844：暴击 → 不回血");
        std::cout << "  [T2720.b] 844 必暴无吸取 通过\n";
    }

    std::cout << "scenario_2720 PASS (J170 未暴吸取族)" << std::endl;
    return 0;
}
