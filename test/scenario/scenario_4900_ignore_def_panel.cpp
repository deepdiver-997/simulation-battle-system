#include <utils/sbs_rand.h>
// 场景 4900 —— K334 无视防御比例族（668，soul_lib/k334_ignore_def_panel.cpp）
//（2026-10-21，worktree ../agents/k334；号段 4900-4909 → T4900-T4902 三块）
//
// 断言口径（拍打 atk=300；对手防御面板 100 → 削 20% = 80）：
//   - T4900 面板白盒：入场后对手 numericalBase[DEF] = 80；红伤带按 def 80 放大
//     [87,102]×100/80 = [108,128]。
//   - T4901 无印对照：def 100 → 红伤带 [87,102]。
//   - T4902 boss 无效：start 前置旗 → 不削（def 100、红伤带不变）。
#include <cstdlib>
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

constexpr int kPaiDa = 10004;
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [20,1]：db[0]=20 无视比例%（db[1] 废位）。
    SoulMark sm(668, "K334 魂印", "test", EffectArgs(std::vector<int>{20, 1}));
    sm.monster_id = 2945;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "超时空暴龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/300, kMaxHp, make_mark())
        : make_pet(1, "裸暴龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/300, kMaxHp);
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

    // ══ T4900 门开：面板削 20% + 红伤放大带 ════════════════════════════════════
    {
        sbs_srand(4900);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::DEFENSE],
                 80,
                 "T4900 668：入场 → 对手防御面板 100×0.8 = 80（实测 def=" +
                     std::to_string(ctx.getPet(1).numericalBase[
                         NumericalPropertyIndex::DEFENSE]) + "）");
        const int drop = slap_round(b);
        CHECK(drop >= 108 && drop <= 128,
              "T4900 668：def 80 → 红伤放大带 [108,128]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4900] 面板削防/红伤放大 通过\n";
    }

    // ══ T4901 无印对照：def 100 红伤带 ═════════════════════════════════════════
    {
        sbs_srand(4901);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::DEFENSE],
                 100, "T4901 668：无印对照 → 对手防御面板不动");
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4901 668：无印 → 红伤带 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4901] 无印对照 通过\n";
    }

    // ══ T4902 boss 无效：不削防 ════════════════════════════════════════════════
    {
        sbs_srand(4902);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::DEFENSE],
                 100, "T4902 668：boss 无效 → 防御面板不削");
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4902 668：boss 无效 → 红伤带 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4902] boss 无效门 通过\n";
    }

    std::cout << "scenario_4900 PASS (K334 无视防御比例族)" << std::endl;
    return 0;
}
