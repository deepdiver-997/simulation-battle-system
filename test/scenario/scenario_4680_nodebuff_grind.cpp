#include <utils/sbs_rand.h>
// 场景 4680 —— K312 非下降态磨血族（655，soul_lib/k335_nodrop_grind.cpp（K312 已收敛））
//（2026-10-21，worktree ../agents/k312；号段 4680-4689 → T4680-T4682 三块）
//
// 断言口径（max=800000 → 磨血 200000；对手 NONE 无扰动；FIXED 无浮动恰值）：
//   - T4680 门开：对手无下降态 → -200000。
//   - T4681 门闭：对手带 -2 → 不磨。
//   - T4682 boss 无效：start 前置旗 → 不磨。
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
    // DB args 原序 [1,4,1]：db[1]=4 分母（余废位）。
    SoulMark sm(655, "K312 魂印", "test", EffectArgs(std::vector<int>{1, 4, 1}));
    sm.monster_id = 2922;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "神武圣皇", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    // ══ T4680 门开：对手无下降态 → -200000 ═════════════════════════════════════
    {
        sbs_srand(4680);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 500000;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 300000,
                 "T4680 655：对手无下降态 → 磨 1/4 max = 200000（实测 hp=" +
                     std::to_string(b.ctx().getPet(1).hp) + "）");
        std::cout << "  [T4680] 门开磨血 通过\n";
    }

    // ══ T4681 门闭：对手带下降 → 不磨 ══════════════════════════════════════════
    {
        sbs_srand(4681);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        (void)stat_drop(&b.ctx(), 1, /*stat=*/0, /*amount=*/2, /*actor=*/0);
        const int hp0 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).hp, hp0,
                 "T4681 655：对手下降态 → 不磨（实测变化 -" +
                     std::to_string(hp0 - b.ctx().getPet(1).hp) + "）");
        std::cout << "  [T4681] 门闭对照 通过\n";
    }

    // ══ T4682 boss 无效：门开不磨 ══════════════════════════════════════════════
    {
        sbs_srand(4682);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int hp0 = b.ctx().getPet(1).hp;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).hp, hp0,
                 "T4682 655：boss 无效 → 不磨（实测变化 -" +
                     std::to_string(hp0 - b.ctx().getPet(1).hp) + "）");
        std::cout << "  [T4682] boss 无效门 通过\n";
    }

    std::cout << "scenario_4680 PASS (K312 非下降态磨血族)" << std::endl;
    return 0;
}
