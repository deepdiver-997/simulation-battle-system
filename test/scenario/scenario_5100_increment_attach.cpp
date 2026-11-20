// 场景 5100 —— K348 攻击递增附加固伤族（807，soul_lib/k_increment_attach.cpp）
//（2026-10-22，worktree k348；号段 5100-5109 → T5100-T5101 两块）
//
// 断言口径（红伤带锚 atk300/def100 raw ∈ [87,102] → [85,104]）：
//   - T5100 连续 5 回合攻击：第 n 次使用附伤 min(100×n, 400) →
//     R1..R5 总差 ∈ [185,204]/[285,304]/[385,404]/[485,504]/[485,504]（封顶档）。
//   - T5101 无印对照：单回合纯红伤 ∈ [85,104]。
//   印主每次只出一刀（拍打），对手 NONE；对手 hp 200000 不会阵亡。
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
constexpr int kHp = 200000;
constexpr int kRedLo = 85;
constexpr int kRedHi = 104;

SoulMark make_mark() {
    // DB args 原序 [100,400,1]：db[0]=首档/增量、db[1]=上限（db[2] 废位）。
    SoulMark sm(807, "K348 魂印", "test", EffectArgs(std::vector<int>{100, 400, 1}));
    sm.monster_id = 3168;
    return sm;
}

ElfPet mark_pet(bool with_mark) {
    ElfPet p = with_mark
        ? make_pet(1, "冥王·灭天截", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/300, kHp, make_mark())
        : make_pet(1, "裸印主", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                   /*speed=*/300, /*atk=*/300, kHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

long long round_delta(ScenarioBattle& b) {
    BattleContext& ctx = b.ctx();
    const int hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return hp0 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5100 递增 5 档：100/200/300/400/400（封顶）════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)), make_party(foe_pet()));
        b.start();
        const int attach[5] = {100, 200, 300, 400, 400};
        std::string measured;
        for (int r = 0; r < 5; ++r) {
            const long long delta = round_delta(b);
            measured += " R" + std::to_string(r + 1) + "=" + std::to_string(delta);
            CHECK(delta >= attach[r] + kRedLo && delta <= attach[r] + kRedHi,
                  "T5100 807：R" + std::to_string(r + 1) + " 附伤 " +
                      std::to_string(attach[r]) + "（总差 " + std::to_string(delta) +
                      "，期望带 [" + std::to_string(attach[r] + kRedLo) + "," +
                      std::to_string(attach[r] + kRedHi) + "]）");
        }
        std::cout << "  [T5100] 807 递增 5 档 通过（" << measured << "）\n";
    }

    // ══ T5101 无印对照：纯红伤 ═══════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(foe_pet()));
        b.start();
        const long long delta = round_delta(b);
        CHECK(delta >= kRedLo && delta <= kRedHi,
              "T5101 807c：无印对照无附伤（总差 " + std::to_string(delta) +
                  "，期望带 [85,104]）");
        std::cout << "  [T5101] 无印对照 通过（delta=" << delta << "）\n";
    }

    std::cout << "scenario_5100 PASS (K348 攻击递增附加固伤族)" << std::endl;
    return 0;
}
