#include <utils/sbs_rand.h>
// 场景 3620 —— K220 概率威力翻倍族（292，soul_lib/k220_roll_power.cpp）
//（2026-10-07，worktree ../agents/k220；号段 3620-3629 → T3620-T3621 两块）
//
// 断言口径（K187 字段断言范式）：停泊 FIRST_SKILL_EFFECT 读视图。
// 6% 掷点 sbs_srand 钉种子（探针 600 扫两支）：落地 → 70（35×2）；失手 → 35。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;

ElfPet mark_pet() {
    std::vector<int> args{0, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(292, "K220 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int probe(int seed) {
    ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(seed);
    b.start();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const int view = ctx.ws.skill_power_view[0].value;
    b.drive();
    return view;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3620 落地 seed：威力 ×2 ══
    {
        CHECK_EQ(probe(72), 70, "T3620 292：落地 seed → 视图 ×2（35→70）");
        std::cout << "  [T3620] 落地翻倍 通过\n";
    }

    // ══ T3621 失手 seed：原威力 ══
    {
        CHECK_EQ(probe(2), 35, "T3621 292：失手 seed → 原威力 35");
        std::cout << "  [T3621] 失手对照 通过\n";
    }

    std::cout << "scenario_3620 PASS (K220 概率威力翻倍族)" << std::endl;
    return 0;
}
