// 场景 3910 —— K244 叠伤成长族（842，soul_lib/k244_stack_amp_growth.cpp）
//（2026-10-13，worktree ../agents/k244；号段 3910-3919 → T3910 一块七回合）
//
// 断言口径（行 [30,150,1]：每层 +30%、上限 150%）：
//   R1 攻（0 层，基线 d1）→ R1 末叠 1 层 → R2 攻（+30%，d2）→ … R6（150% 满层）
//   → R7（cap 平台）。
//   · crit 置零，浮带只剩 85%~100%：d2 严格 > d1（min 1.105×base > max 1.0×base）；
//   · d6 严格 > d4（2 层差 ≥60% 压过浮带）；
//   · d7 与 d6 同分布（cap 平台，宽松带 [0.7,1.3]×）；
//   · 对手全程 NONE：其每回合失血 = 印主红伤（唯一变量）。
//   ⚠️ 纯 drive() 流（3720 记档）。
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
constexpr int kMaxHp = 99999;

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/100, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(842, "K244 魂印", "",
                                               EffectArgs(std::vector<int>{30, 150, 1})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    set_crit_sixteenths(p, 0);
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int mark_attacks(ScenarioBattle& b) {
    const int before = b.pet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return before - b.pet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3910 叠伤成长七回合 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();

        const int d1 = mark_attacks(b);   // R1：0 层基线
        CHECK(d1 > 0, "T3910 842：R1 基线伤害");
        const int d2 = mark_attacks(b);   // R2：1 层 +30%
        CHECK(d2 > d1, "T3910 842：R2 增伤压过浮带（d2=" + std::to_string(d2)
                           + " > d1=" + std::to_string(d1) + "）");
        const int d3 = mark_attacks(b);   // R3：2 层
        const int d4 = mark_attacks(b);   // R4：3 层
        const int d5 = mark_attacks(b);   // R5：4 层
        const int d6 = mark_attacks(b);   // R6：5 层 = 150% 满层
        CHECK(d6 > d4, "T3910 842：R6（150%）严格 > R4（90%）（d6=" + std::to_string(d6)
                           + " > d4=" + std::to_string(d4) + "）");
        const int d7 = mark_attacks(b);   // R7：cap 平台
        CHECK(d7 >= d6 * 7 / 10 && d7 <= d6 * 13 / 10,
              "T3910 842：R7 cap 平台与 R6 同分布（d6=" + std::to_string(d6)
                  + " d7=" + std::to_string(d7) + "）");
        std::cout << "  [T3910] 叠伤成长七回合 通过（d1=" << d1 << " d2=" << d2
                  << " d4=" << d4 << " d6=" << d6 << " d7=" << d7 << "）\n";
    }

    std::cout << "scenario_3910 PASS (K244 叠伤成长族)" << std::endl;
    return 0;
}
