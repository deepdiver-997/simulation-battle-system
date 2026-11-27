#include <utils/sbs_rand.h>
// 场景 2891 —— K166 effect 283：致命一击伤害 +10%（isCrit 门 AMP）
//
// 断言口径（srand 对齐 → AMP 公式内取整同 203 先例可精确）：
//   A 必暴（set_crit_sixteenths 16）→ 标记组伤害 == 基线 ×110/100（db[0]=10）。
//   B 非暴（rate 0）→ 零行为（isCrit 门）。
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

ElfPet holder_pet(int crit_sixteenths, bool with_mark) {
    ElfPet p = scenario::make_pet(1, "持有方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000,
                                  with_mark ? make_mark(283, {10, 0}, 2524) : SoulMark{});
    scenario::set_crit_sixteenths(p, crit_sixteenths);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}
ElfPet dummy_pet() {
    return scenario::make_pet(2, "木桩", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

int probe_loss(int crit_sixteenths, bool with_mark) {
    ScenarioBattle b(scenario::make_party(holder_pet(crit_sixteenths, with_mark)),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
    b.start();
    const int hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const int base = probe_loss(16, false);
    CHECK(base > 1000, "必暴基线量级合理");
    CHECK_EQ(probe_loss(16, true), base * 110 / 100, "A 必暴 → +10%（AMP 精确）");
    {
        const int no_crit = probe_loss(0, false);
        CHECK_EQ(probe_loss(0, true), no_crit, "B 非暴 → isCrit 门关（零行为）");
    }

    std::cout << "scenario_2891 PASS (K166 effect283 暴击伤 AMP)" << std::endl;
    return 0;
}
