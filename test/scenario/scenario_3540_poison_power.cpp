#include <utils/sbs_rand.h>
// 场景 3540 —— K212 对手中毒威力翻倍族（308，soul_lib/k212_poison_power.cpp）
//（2026-10-07，worktree ../agents/k212；号段 3540-3549 → T3540-T3541 两块）
//
// 断言口径（K187 字段断言范式）：停泊 FIRST_SKILL_EFFECT 读视图。
// 基线 = 撞击 35；对手中毒 → 70；门闭（无异常）→ 35。
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
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);

ElfPet mark_pet() {
    std::vector<int> args{0, 0, 2, 1, 100};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(308, "K212 魂印", "",
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

int probe(bool poisoned) {
    ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    if (poisoned) {
        REQUIRE(try_apply_anomaly(&ctx, 1, kPoison, /*duration=*/3, /*actor=*/0));
    }
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

    // ══ T3540 对手中毒 → 威力 ×2 ══
    {
        CHECK_EQ(probe(/*poisoned=*/true), 70, "T3540 308：对手中毒 → 威力视图 ×2（35→70）");
        std::cout << "  [T3540] 中毒门翻倍 通过\n";
    }

    // ══ T3541 门闭：无异常 → 原威力 ══
    {
        CHECK_EQ(probe(/*poisoned=*/false), 35, "T3541 308：门闭 → 原威力 35");
        std::cout << "  [T3541] 门闭对照 通过\n";
    }

    std::cout << "scenario_3540 PASS (K212 对手中毒威力翻倍族)" << std::endl;
    return 0;
}
