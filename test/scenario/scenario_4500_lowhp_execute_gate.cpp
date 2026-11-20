#include <utils/sbs_rand.h>
// 场景 4500 —— K294 低体瞬杀门族（625，soul_lib/k294_lowhp_execute_gate.cpp）
//（2026-10-21，worktree ../agents/k294；号段 4500-4509 → T4500-T4502 三块）
//
// 断言口径（全程 idle 回合：无蹭伤，门限边界可精确；max=800000 → 门限 200000）：
//   - T4500 门闭：对手满血 / 恰 200000（=25%，"低于"不含等号）→ 存活。
//   - T4501 门开：对手 199999 → 瞬杀（hp=0）；再验证 boss 无效（门开不杀）。
//   （不设无印对照：门闭块即对照。）
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
    // DB args 原序 [25,1]：db[0]=25 门限%（db[1] 不消费）。
    SoulMark sm(625, "K294 魂印", "test", EffectArgs(std::vector<int>{25, 1}));
    sm.monster_id = 2882;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "马尔修斯（毁灭型）", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    // ══ T4500 门闭：满血 / 恰 25% → 存活 ═══════════════════════════════════════
    {
        sbs_srand(4500);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        idle_round(b);   // 满血
        CHECK_EQ(b.ctx().getPet(1).hp, kMaxHp,
                 "T4500 625：满血 → 门闭存活");
        b.ctx().getPet(1).hp = 200000;   // 恰 = 25%（"低于"不含等号）
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 200000,
                 "T4500 625：恰 25% → 非'低于'，存活（实测 hp=" +
                     std::to_string(b.ctx().getPet(1).hp) + "）");
        std::cout << "  [T4500] 门闭/边界严格 通过\n";
    }

    // ══ T4501 门开：199999 → 瞬杀 ══════════════════════════════════════════════
    {
        sbs_srand(4501);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 199999;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 0,
                 "T4501 625：hp=199999 < 25% → 瞬杀（实测 hp=" +
                     std::to_string(b.ctx().getPet(1).hp) + "）");
        std::cout << "  [T4501] 门开瞬杀 通过\n";
    }

    // ══ T4502 boss 无效：门开不杀 ══════════════════════════════════════════════
    //（boss_invalid 在节点进桶口过滤（soul_mark_manager:505）→ 旗必须 start 前置位）
    {
        sbs_srand(4502);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(1).hp = 199999;
        idle_round(b);
        CHECK_EQ(b.ctx().getPet(1).hp, 199999,
                 "T4502 625：boss 无效 → 门开也不杀（实测 hp=" +
                     std::to_string(b.ctx().getPet(1).hp) + "）");
        std::cout << "  [T4502] boss 无效门 通过\n";
    }

    std::cout << "scenario_4500 PASS (K294 低体瞬杀门族)" << std::endl;
    return 0;
}
