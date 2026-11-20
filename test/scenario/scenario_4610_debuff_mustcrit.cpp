#include <utils/sbs_rand.h>
// 场景 4610 —— K305 能力下降门必暴族（290，soul_lib/k305_debuff_mustcrit.cpp）
//（2026-10-21，worktree ../agents/k305；号段 4610-4619 → T4610-T4612 三块）
//
// 断言口径（拍打 atk=300 → 红伤带 [87,102]，必暴 ×2（实测锚，T4611 首跑 190 勘误
// 自 ×1.5 假设）→ [174,204]，两带不相交；场景直调 stat_drop 给对手挂下降态；对手 NONE）：
//   - T4610 门闭：对手满状态 → 纯红伤带。
//   - T4611 门开：对手 -2 → 必暴带。
//   - T4612 无印对照：对手 -2 → 仍纯红伤带。
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

SoulMark make_mark() {
    // DB args 原序 [3,100]：db[1]=100 必暴%（db[0] 记档不消费）。
    SoulMark sm(290, "K305 魂印", "test", EffectArgs(std::vector<int>{3, 100}));
    sm.monster_id = 3248;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "黄金圣龙王", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/300, 800000, make_mark())
        : make_pet(1, "裸圣龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/300, 800000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, 800000);
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

    // ══ T4610 门闭：对手满状态 → 纯红伤 ════════════════════════════════════════
    {
        sbs_srand(4610);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4610 290：门闭 → 红伤带 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4610] 门闭对照 通过\n";
    }

    // ══ T4611 门开：对手 -2 → 必暴带 ═══════════════════════════════════════════
    {
        sbs_srand(4611);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        (void)stat_drop(&b.ctx(), 1, /*stat=*/0, /*amount=*/2, /*actor=*/0);
        const int drop = slap_round(b);
        CHECK(drop >= 174 && drop <= 204,
              "T4611 290：对手下降态 → 必暴带 [174,204]（实测 " +
                  std::to_string(drop) + "）");
        std::cout << "  [T4611] 门开必暴 通过\n";
    }

    // ══ T4612 无印对照：对手 -2 仍纯红伤 ═══════════════════════════════════════
    {
        sbs_srand(4612);
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        (void)stat_drop(&b.ctx(), 1, /*stat=*/0, /*amount=*/2, /*actor=*/0);
        const int drop = slap_round(b);
        CHECK(drop >= 87 && drop <= 102,
              "T4612 290：无印对照 → 红伤带 [87,102]（实测 " + std::to_string(drop) +
                  "）");
        std::cout << "  [T4612] 无印对照 通过\n";
    }

    std::cout << "scenario_4610 PASS (K305 能力下降门必暴族)" << std::endl;
    return 0;
}
