#include <utils/sbs_rand.h>
// 场景 3610 —— K218 首回合先制族（277，soul_lib/k218_entry_priority.cpp）
//（2026-10-07，worktree ../agents/k218；号段 3610-3619 → T3610-T3611 两块）
//
// 断言口径：印主速度 100 vs 对手 101（速度必输）——登场回合双方 SELECT_SKILL 出手
// → pr 翻成印主侧（先制+1 生效）；次回合（非首回合）速度回归 → pr 归对手。
// 无印对照：登场回合 pr 即对手侧。
#include <array>
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

constexpr int kJi = 10001;

ElfPet mark_pet(bool with_mark) {
    std::vector<int> args{0, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(with_mark ? 277 : 0, "K218 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/101, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_attack(ScenarioBattle& b) {
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

    // ══ T3610 登场回合 pr 翻转；次回合回归速度序 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/true)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(7);
        both_attack(b);   // R1：登场首回合
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_1,
                 "T3610 277：登场首回合 → 先制+1 翻转 pr（速度 100 压过 101）");
        sbs_srand(7);
        both_attack(b);   // R2：非首回合
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T3610 277：次回合 → 先制加成失效，速度序回归");
        std::cout << "  [T3610] 登场首回合先制 通过\n";
    }

    // ══ T3611 无印对照：登场回合 pr 即对手侧 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*with_mark=*/false)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(7);
        both_attack(b);
        CHECK_EQ(ctx.preemptive_right, PreemptiveRight::SEER_ROBOT_2,
                 "T3611 277：无印对照 → 速度序（对手先）");
        std::cout << "  [T3611] 无印对照 通过\n";
    }

    std::cout << "scenario_3610 PASS (K218 首回合先制族)" << std::endl;
    return 0;
}
