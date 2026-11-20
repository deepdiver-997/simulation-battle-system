// 场景 3670 —— K223 首回合减半+短窗附伤族（264，soul_lib/k223_halve_attach.cpp）
//（2026-10-07，worktree ../agents/k223；号段 3670-3679 → T3670-T3672 三块）
//
// 断言口径（raw 台账差账）：
//   R1：我挨打减半（loss == raw×50/100）+ 我攻附 100（loss == raw+100）；
//   R2：减半过期（loss == raw）+ 附伤仍在（loss == raw+100）；
//   R3：附伤过期（loss == raw）。
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
                        /*soul_mark=*/SoulMark(264, "K223 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/50, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void exchange(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    // ══ T3670 R1：减半 + 附伤 ══
    {
        const int my0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        exchange(b);
        CHECK_EQ(my0 - ctx.getPet(0).hp, ctx.ws.raw_attack_damage[1] * 50 / 100,
                 "T3670 264：R1 受击减半（raw×50/100）");
        CHECK_EQ(foe0 - ctx.getPet(1).hp, ctx.ws.raw_attack_damage[0] + 100,
                 "T3670 264：R1 攻附 100 固定");
        std::cout << "  [T3670] R1 减半+附伤 通过\n";
    }

    // ══ T3671 R2：减半过期 + 附伤仍在 ══
    {
        const int my0 = ctx.getPet(0).hp;
        const int foe0 = ctx.getPet(1).hp;
        exchange(b);
        CHECK_EQ(my0 - ctx.getPet(0).hp, ctx.ws.raw_attack_damage[1],
                 "T3671 264：R2 减半过期（原伤）");
        CHECK_EQ(foe0 - ctx.getPet(1).hp, ctx.ws.raw_attack_damage[0] + 100,
                 "T3671 264：R2 附伤仍在（2 回合窗）");
        std::cout << "  [T3671] R2 减半过期附伤仍在 通过\n";
    }

    // ══ T3672 R3：附伤过期 ══
    {
        const int foe0 = ctx.getPet(1).hp;
        exchange(b);
        CHECK_EQ(foe0 - ctx.getPet(1).hp, ctx.ws.raw_attack_damage[0],
                 "T3672 264：R3 附伤过期（原伤）");
        std::cout << "  [T3672] R3 附伤过期 通过\n";
    }

    std::cout << "scenario_3670 PASS (K223 首回合减半+短窗附伤族)" << std::endl;
    return 0;
}
