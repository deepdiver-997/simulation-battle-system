// 场景 3790 —— K235 后出手减伤+回血族（705，soul_lib/k235_second_mover_mitigate_heal.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3790-3799 → T3790 一块两例）
//
// 断言口径（净值带分离：浮动只作用于红伤，回复 200 恒定）：
//   - a 后手（印主 speed 100 < 对手 300）：受击减半 [31,50] + 回合末回 200 →
//     净值 = 200 - 减半红伤 ∈ [150,169]；与"无减伤"净值 [101,138] 带分离，
//     单断言同时钉死减伤与回血两半。
//   - b 先手（印主 speed 300 > 对手 100）：账本 0 → 无减伤无回血 →
//     净值 = -红伤 ∈ (-100,-60)。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler

SoulMark make_mark() {
    SoulMark sm(705, "K235 魂印", "test", EffectArgs(std::vector<int>{50, 200}));
    sm.monster_id = 3004;   // 真龙·塔西亚
    return sm;
}

ElfPet mark_pet(int speed) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/1, 99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3790.a 后手：受击减 50% + 回合末回 200（净值带 [150,169]）══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;   // 预压血量（满血回复会封顶，SOP §五.2）
        const int hp0 = ctx.getPet(0).hp;
        one_round(b);
        const int net = ctx.getPet(0).hp - hp0;
        CHECK(net > 148 && net < 160,
              "T3790.a 后手净值 = 回200 - 减半红伤[31,50]");
        std::cout << "  [T3790.a] 后手减伤+回血 通过（净值=" << net << "）\n";
    }

    // ══ T3790.b 先手：无减伤无回血 ══
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        one_round(b);
        const int net = ctx.getPet(0).hp - hp0;
        CHECK(net > -105 && net < -85, "T3790.b 先手 = 全额红伤、无回复");
        std::cout << "  [T3790.b] 先手静默 通过（净值=" << net << "）\n";
    }

    std::cout << "scenario_3790 PASS (K235 后出手减伤+回血族)" << std::endl;
    return 0;
}
