#include <utils/sbs_rand.h>
// 场景 4370 —— K281 回合回血+低血翻倍族（610，soul_lib/k277_round_heal_double.cpp（K281 已收敛））
//（2026-10-21，worktree ../agents/k281；号段 4370-4379 → T4370-T4372 三块）
//
// 断言口径（max=1000000 → 常态回血 200000、翻倍 400000；对手全程 NONE 无扰动）：
//   - T4370 常态：hp=500000（≥1/3）→ +200000 = 700000。
//   - T4371 翻倍：hp=300000（×3 < max）→ +400000 = 700000。
//   - T4372 边界严格 + 钳顶：hp=333334（×3 ≥ max，非"少于"）→ +200000；
//     hp=333333（×3 < max）→ 翻倍；hp=999000 → 钳 max=1000000。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kMaxHp = 1000000;

SoulMark make_mark() {
    // DB args 原序 [5,3]：db[0]=5（回血分母）、db[1]=3（翻倍门分母）。
    SoulMark sm(610, "K281 魂印", "test", EffectArgs(std::vector<int>{5, 3}));
    sm.monster_id = 2849;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "昊希娜", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4370 常态回血 1/5 ═════════════════════════════════════════════════════
    {
        sbs_srand(4370);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 500000;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 700000,
                 "T4370 610：hp≥1/3 → 回血 1/5 = 200000（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4370] 常态回血 通过\n";
    }

    // ══ T4371 低体翻倍 ═════════════════════════════════════════════════════════
    {
        sbs_srand(4371);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 300000;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 700000,
                 "T4371 610：hp<1/3 → 翻倍 400000（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4371] 低体翻倍 通过\n";
    }

    // ══ T4372 边界严格 + 钳顶 ══════════════════════════════════════════════════
    {
        sbs_srand(4372);
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 333334;   // ×3 = 1000002 ≥ max → 非"少于"
            idle_round(b);
            CHECK_EQ(b.ctx().getPet(0).hp, 533334,
                     "T4372 610：hp=333334 恰≥1/3 → 不翻倍（实测 hp=" +
                         std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 333333;   // ×3 = 999999 < max → 翻倍
            idle_round(b);
            CHECK_EQ(b.ctx().getPet(0).hp, 733333,
                 "T4372 610：hp=333333 恰<1/3 → 翻倍（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
            b.start();
            b.ctx().getPet(0).hp = 999000;   // 回血钳 max
            idle_round(b);
            CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp,
                 "T4372 610：回血钳 max（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        }
        std::cout << "  [T4372] 边界严格/钳顶 通过\n";
    }

    std::cout << "scenario_4370 PASS (K281 回合回血+低血翻倍族)" << std::endl;
    return 0;
}
