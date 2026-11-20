#include <utils/sbs_rand.h>
// 场景 4640 —— K308 击败传染异常族（309，soul_lib/k308_kill_poison_inherit.cpp）
//（2026-10-21，worktree ../agents/k308；号段 4640-4649 → T4640-T4641 两块）
//
// 断言口径（印主 atk=999 一刀必杀 hp=100 对手；对手队 6 只同体）：
//   - T4640 传染：预置对手中毒 → 击杀 → 继任对手入场即带中毒。
//   - T4641 门闭对照：击杀未中毒对手 → 继任者干净。
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
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);
constexpr int kFoeHp = 100;

SoulMark make_mark() {
    // DB args 原序 [1,0]：db[0]=1 中毒码（db[1] 废位）。
    SoulMark sm(309, "K308 魂印", "test", EffectArgs(std::vector<int>{1, 0}));
    sm.monster_id = 2618;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "幻草灵狮", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/999, 800000, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kFoeHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void kill_round(ScenarioBattle& b) {
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

    // ══ T4640 传染：击杀中毒对手 → 继任者带毒 ══════════════════════════════════
    {
        sbs_srand(4640);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        apply_anomaly(&ctx, 1, kPoison, /*duration_rounds=*/9, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kPoison));
        kill_round(b);   // 印主一刀击杀中毒对手 → 继任者入场
        CHECK(ctx.on_stage[1] != 0 || ctx.getPet(1).hp != 100,
              "T4640 309：首只对手确已被击杀（前置自检）");
        CHECK(ctx.has_active_abnormal_status(1, kPoison),
              "T4640 309：下一个出场的对手进入中毒状态");
        std::cout << "  [T4640] 击败传染 通过\n";
    }

    // ══ T4641 门闭对照：击杀未中毒对手 → 继任者干净 ════════════════════════════
    {
        sbs_srand(4641);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        kill_round(b);
        CHECK(!b.ctx().has_active_abnormal_status(1, kPoison),
              "T4641 309：击杀未中毒对手 → 继任者不带毒");
        std::cout << "  [T4641] 门闭对照 通过\n";
    }

    std::cout << "scenario_4640 PASS (K308 击败传染异常族)" << std::endl;
    return 0;
}
