#include <utils/sbs_rand.h>
// 场景 3560 —— K213 强化攻击麻痹族（316，soul_lib/k213_boost_paralyze.cpp）
//（2026-10-07，worktree ../agents/k213；号段 3560-3569 → T3560-T3562 三块）
//
// 断言口径：sbs_srand 钉种子分岔（探针标定 15% 掷点）。强化门三向：
// 有提升 → 落地麻痹；无提升 → 不掷（同 seed 不动）；落地/失手对照。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);

ElfPet mark_pet() {
    std::vector<int> args{0, 0, 2, 15};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(316, "K213 魂印", "",
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

void attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3560 强化 + 落地 seed：对手麻痹 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;   // 自身强化
        b.ctx().seed_rng(58);
        attack_round(b);
        CHECK(ctx.has_active_abnormal_status(1, kParalysis),
              "T3560 316：强化+落地 seed → 对手麻痹");
        std::cout << "  [T3560] 强化攻击麻痹 通过\n";
    }

    // ══ T3561 失手 seed：无麻痹 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = 1;
        b.ctx().seed_rng(2);
        attack_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T3561 316：失手 seed → 无麻痹");
        std::cout << "  [T3561] 失手对照 通过\n";
    }

    // ══ T3562 无强化门闭：不掷（无麻痹）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.ctx().seed_rng(1);
        attack_round(b);
        CHECK(!ctx.has_active_abnormal_status(1, kParalysis),
              "T3562 316：无强化 → 门闭不掷");
        std::cout << "  [T3562] 强化门闭对照 通过\n";
    }

    std::cout << "scenario_3560 PASS (K213 强化攻击麻痹族)" << std::endl;
    return 0;
}
