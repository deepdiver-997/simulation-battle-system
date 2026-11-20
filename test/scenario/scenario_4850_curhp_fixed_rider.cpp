#include <utils/sbs_rand.h>
// 场景 4850 —— K329 当前生命比例固伤族（783，soul_lib/k329_curhp_fixed_rider.cpp）
//（2026-10-21，worktree ../agents/k329；号段 4850-4859 → T4850-T4852 三块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]；max=800000 → 满血 30% = 240000；
// 对手 NONE；与 960 已损口径互辨——本行满血时固伤最大、半血时减半）：
//   - T4850 满血：掉血 = 240000 + 红伤（恰带 [240087,240102]）。
//   - T4851 半血：30%×400000 = 120000 + 红伤（恰带 [120087,120102]——960 同场景
//     是 240000，方向相反，辨析锚）。
//   - T4852 boss 无效：只红伤。
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
    // DB args 原序 [30,1]：db[0]=30 现血比例%（db[1] 废位）。
    SoulMark sm(783, "K329 魂印", "test", EffectArgs(std::vector<int>{30, 1}));
    sm.monster_id = 3135;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "幽冥邪龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark());
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

int slap_round(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int foe0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4850 满血：固伤 240000 + 红伤 ═════════════════════════════════════════
    {
        sbs_srand(4850);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        const int drop = slap_round(b);
        CHECK(drop >= 240087 && drop <= 240102,
              "T4850 783：满血 → 固伤 240000+红伤（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4850] 满血固伤 通过\n";
    }

    // ══ T4851 半血：固伤 120000 + 红伤（与 960 辨析：960 半血=满损 240000 反向）══
    {
        sbs_srand(4851);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 400000;
        const int drop = slap_round(b);
        CHECK(drop >= 120087 && drop <= 120102,
              "T4851 783：半血 → 固伤 120000+红伤（实测 " + std::to_string(drop) +
                  "；若误按 960 已损口径会 ≈240095）");
        std::cout << "  [T4851] 半血辨析锚 通过\n";
    }

    // ══ T4852 boss 无效：只红伤 ════════════════════════════════════════════════
    {
        sbs_srand(4852);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4852 783：boss 无效 → 只红伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4852] boss 无效门 通过\n";
    }

    std::cout << "scenario_4850 PASS (K329 当前生命比例固伤族)" << std::endl;
    return 0;
}
