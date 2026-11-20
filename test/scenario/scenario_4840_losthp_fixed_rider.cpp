#include <utils/sbs_rand.h>
// 场景 4840 —— K328 已损失体力比例固伤族（960，soul_lib/k328_losthp_fixed_rider.cpp）
//（2026-10-21，worktree ../agents/k328；号段 4840-4849 → T4840-T4842 三块）
//
// 断言口径（拍打 atk=300 → 红伤 [87,102]；max=800000 → 半血已损 400000 → 固伤 120000；
// 对手 NONE）：
//   - T4840 半血：掉血 = 120000 + 红伤（恰带 [120087,120102]）。
//   - T4841 满血：已损 0 → 只红伤。
//   - T4842 boss 无效：只红伤。
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
    // DB args 原序 [30,1]：db[0]=30 已损比例%（db[1] 废位）。
    SoulMark sm(960, "K328 魂印", "test", EffectArgs(std::vector<int>{30, 1}));
    sm.monster_id = 3357;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "伊基塔", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    // ══ T4840 半血：固伤 120000 + 红伤 ═════════════════════════════════════════
    {
        sbs_srand(4840);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(1).hp = 400000;   // 已损 400000 → 30% = 120000
        const int drop = slap_round(b);
        CHECK(drop >= 120087 && drop <= 120102,
              "T4840 960：半血 → 固伤 120000+红伤（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4840] 半血固伤 通过\n";
    }

    // ══ T4841 满血：无已损 → 只红伤 ════════════════════════════════════════════
    {
        sbs_srand(4841);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4841 960：满血已损 0 → 只红伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4841] 满血对照 通过\n";
    }

    // ══ T4842 boss 无效：半血也只红伤 ══════════════════════════════════════════
    {
        sbs_srand(4842);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(1).hp = 400000;
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4842 960：boss 无效 → 只红伤 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4842] boss 无效门 通过\n";
    }

    std::cout << "scenario_4840 PASS (K328 已损失体力比例固伤族)" << std::endl;
    return 0;
}
