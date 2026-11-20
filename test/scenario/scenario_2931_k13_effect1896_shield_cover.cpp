#include <utils/sbs_rand.h>
// 场景 2931 —— K13 effect 1896 未满分支（盾/罩 + <1/2 额外 + 克罗恩全量 PP）
//
// 断言口径：
//   A 图托尔斯未满（40% 体）→ 盾 300 / 罩 300（200+100 额外并入）。
//   B 未满但 ≥1/2（60%）→ 盾 200 / 罩 200（无额外）。
//   C 不叠加：两回合后仍 300/300（max 语义，非累加）。
//   D 克罗恩 4609 未满 → 盾 0 / 罩 200；<1/2 → 盾 150 / 罩 350；满体 → 全 PP -2。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet(std::vector<int> db_args, int monster_id) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(1896, std::move(db_args), monster_id));
}
ElfPet dummy_pet() {
    return scenario::make_pet(2, "木桩", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

struct ShieldProbe {
    int shield = 0;
    int cover = 0;
};

ShieldProbe probe_shield(std::vector<int> db_args, int monster_id, int hp_pct_permille,
                         int rounds) {
    ScenarioBattle b(scenario::make_party(holder_pet(std::move(db_args), monster_id)),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp =
        1000000 * hp_pct_permille / 1000;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    const ElfPet& p = ctx.seerRobot[0].elfPets[ctx.on_stage[0]];
    return {p.shield, p.cover};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 图托尔斯 <1/2：盾 300 / 罩 300
    {
        const ShieldProbe p = probe_shield({3, 1, 50, 200, 200, 2, 100, 1}, 4311, 400, 1);
        CHECK_EQ(p.shield, 300, "A1 <1/2 → 盾 200+100");
        CHECK_EQ(p.cover, 300, "A2 <1/2 → 罩 200+100");
    }
    // B 图托尔斯 ≥1/2：盾 200 / 罩 200
    {
        const ShieldProbe p = probe_shield({3, 1, 50, 200, 200, 2, 100, 1}, 4311, 600, 1);
        CHECK_EQ(p.shield, 200, "B1 ≥1/2 → 盾 200");
        CHECK_EQ(p.cover, 200, "B2 ≥1/2 → 罩 200");
    }
    // C 不叠加（两回合仍 300/300）
    {
        const ShieldProbe p = probe_shield({3, 1, 50, 200, 200, 2, 100, 1}, 4311, 400, 2);
        CHECK_EQ(p.shield, 300, "C1 两回合不叠加（盾）");
        CHECK_EQ(p.cover, 300, "C2 两回合不叠加（罩）");
    }
    // D 克罗恩：<1/2 → 盾150/罩350；满体 → 全 PP-2
    {
        const ShieldProbe p = probe_shield({5, 2, 0, 0, 200, 2, 150, 1}, 4609, 400, 1);
        CHECK_EQ(p.shield, 150, "D1 克罗恩 <1/2 → 盾 0+150");
        CHECK_EQ(p.cover, 350, "D2 克罗恩 <1/2 → 罩 200+150");
    }
    {
        ScenarioBattle b(scenario::make_party(holder_pet({5, 2, 0, 0, 200, 2, 150, 1}, 4609)),
                         scenario::make_party(dummy_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        int pp0 = 0;
        for (const Skills& s : ctx.seerRobot[1].elfPets[ctx.on_stage[1]].skills) {
            pp0 += s.pp;
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        int pp1 = 0;
        for (const Skills& s : ctx.seerRobot[1].elfPets[ctx.on_stage[1]].skills) {
            pp1 += s.pp;
        }
        CHECK_EQ(pp0 - pp1, 10, "D3 克罗恩满体 → 对手 5 技 PP 各 -2");
    }

    std::cout << "scenario_2931 PASS (K13 effect1896 未满分支/不叠加)" << std::endl;
    return 0;
}
