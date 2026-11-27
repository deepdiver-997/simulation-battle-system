#include <utils/sbs_rand.h>
// 场景 3500 —— K209 受伤转化体力族（326，soul_lib/k209_damage_to_heal.cpp）
//（2026-10-07，worktree ../agents/k209；号段 3500-3509 → T3500-T3501 两块）
//
// 断言口径：压血 5000（deficit 充裕）后每回合只吃对手一刀（自己 NONE）——
//   成功 seed：RC 转化 → 净 0（挨多少回多少）；失败 seed：净 -D。
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
    std::vector<int> args{30, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(326, "K209 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/50, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_hits(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const int kSeedHit = 3;    // 探针标定：转化成功（净 0）
    const int kSeedMiss = 1;   // 探针标定：未触发（净 -D）

    // ══ T3500 转化成功：挨多少回多少（净 0）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        b.ctx().seed_rng(kSeedHit);
        foe_hits(b);
        CHECK_EQ(ctx.getPet(0).hp, my0, "T3500 326：转化成功 → 净 0（受伤=回体）");
        std::cout << "  [T3500] 转化成功 通过\n";
    }

    // ══ T3501 未触发：净 -D（挨打原样）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        b.ctx().seed_rng(kSeedMiss);
        foe_hits(b);
        const int net = ctx.getPet(0).hp - my0;
        CHECK(net < 0, "T3501 326：未触发 → 净掉血（原样受伤）");
        std::cout << "  [T3501] 未触发对照 通过\n";
    }

    std::cout << "scenario_3500 PASS (K209 受伤转化体力族)" << std::endl;
    return 0;
}
