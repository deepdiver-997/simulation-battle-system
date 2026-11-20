#include <utils/sbs_rand.h>
// 场景 4920 —— K336 回合末固伤+随机三项族（690，soul_lib/k336_roundend_fixed_randstat.cpp）
//（2026-10-21，worktree ../agents/k336；号段 4920-4929 → T4920 一块三断言）
//
// 断言口径（对手 NONE；FIXED 无浮动）：
//   R1 末：对手 -200 恰值；自身 6 槽等级和 = 3 且恰好 3 个槽为 +1（互异直证）。
//   R2 末：对手再 -200；等级和 = 6（随机可叠同槽 → 和线性、槽数 ≥3）。
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
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [200,3,1]：db[0]=200 固伤、db[1]=3 项数、db[2]=1 增量。
    SoulMark sm(690, "K336 魂印", "test", EffectArgs(std::vector<int>{200, 3, 1}));
    sm.monster_id = 2983;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "瀚海界王", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    sbs_srand(4920);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;

    idle_round(b);   // R1
    CHECK_EQ(ctx.getPet(1).hp, foe0 - 200,
             "T4920 690：R1 附加固伤恰 200（实测 -" +
                 std::to_string(foe0 - ctx.getPet(1).hp) + "）");
    int sum = 0;
    int raised = 0;
    for (int i = 0; i < 6; ++i) {
        sum += ctx.ability_levels[0][i];
        if (ctx.ability_levels[0][i] > 0) {
            ++raised;
        }
    }
    CHECK_EQ(sum, 3, "T4920 690：R1 随机 3 项各 +1 → 等级和 = 3（实测 " +
                         std::to_string(sum) + "）");
    CHECK_EQ(raised, 3,
             "T4920 690：恰好 3 个槽被提升（互异直证，实测 " +
                 std::to_string(raised) + "）");

    idle_round(b);   // R2
    CHECK_EQ(ctx.getPet(1).hp, foe0 - 400,
             "T4920 690：R2 再 -200（实测 -" +
                 std::to_string(foe0 - ctx.getPet(1).hp) + "）");
    sum = 0;
    for (int i = 0; i < 6; ++i) {
        sum += ctx.ability_levels[0][i];
    }
    CHECK_EQ(sum, 6, "T4920 690：R2 累计等级和 = 6（实测 " + std::to_string(sum) +
                         "）");

    std::cout << "scenario_4920 PASS (K336 回合末固伤+随机三项族)" << std::endl;
    return 0;
}
