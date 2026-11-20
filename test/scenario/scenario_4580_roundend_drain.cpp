#include <utils/sbs_rand.h>
// 场景 4580 —— K302 回合末吸取族（735，soul_lib/k302_roundend_drain.cpp）
//（2026-10-21，worktree ../agents/k302；号段 4580-4589 → T4580-T4581 两块）
//
// 断言口径（对手 NONE；FIXED 粉伤无浮动 → 恰值断言）：
//   - T4580 常态：对手 100000 → 回合末对手 -150、自身 +150（hp=100000 起测）。
//   - T4581 封顶：对手 100 → 吸取 min(150,100)=100（对手 0、自身 +100）。
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
    // DB args 原序 [150,0]：db[0]=150 吸取点数（db[1] 废位）。
    SoulMark sm(735, "K302 魂印", "test", EffectArgs(std::vector<int>{150, 0}));
    sm.monster_id = 3050;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "风玄灵尊", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    // ══ T4580 常态：回合末 -150/+150 ═══════════════════════════════════════════
    {
        sbs_srand(4580);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        ctx.getPet(1).hp = 100000;
        idle_round(b);
        CHECK_EQ(ctx.getPet(1).hp, 99850,
                 "T4580 735：对手回合末 -150（实测 hp=" +
                     std::to_string(ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.getPet(0).hp, 100150,
                 "T4580 735：自身等量恢复 +150（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4580] 常态吸取 通过\n";
    }

    // ══ T4581 封顶：对手血不足 150 → 取存量 ════════════════════════════════════
    {
        sbs_srand(4581);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 100000;
        ctx.getPet(1).hp = 100;
        idle_round(b);
        CHECK_EQ(ctx.getPet(1).hp, 0,
                 "T4581 735：对手 100 < 150 → 吸干至 0（实测 hp=" +
                     std::to_string(ctx.getPet(1).hp) + "）");
        CHECK_EQ(ctx.getPet(0).hp, 100100,
                 "T4581 735：自身按存量恢复 +100（实测 hp=" +
                     std::to_string(ctx.getPet(0).hp) + "）");
        std::cout << "  [T4581] 存量封顶 通过\n";
    }

    std::cout << "scenario_4580 PASS (K302 回合末吸取族)" << std::endl;
    return 0;
}
