// 场景 3660 —— J185 对手降态威力提升族（1953，moves_lib/j_dropcond_power_up.cpp）
//（2026-10-07，worktree ../agents/j185；号段 3660-3669 → T3660-T3661 两块）
//
// 断言口径（威力 view 改写族伤害比例不可精确对齐——SOP 断言方法论 1，85%~100%
// 浮动用 3 场平均比值消除随机：门开(对手任一槽 -1)/门关各 3 场。
// pct=100 载体 37436 → 威力 ×(100+100)/100 = **2 倍**，期望比值 2.0；
// 3 场平均带宽 [1.8, 2.2]）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 37436;   // 未抵的本相：1953[100]，链 1192:0（零参前置哑）

ElfPet carrier_pet() {
    ElfPet p = make_pet(1, "印主", {kCarrier, 10004, 10004, 10004, 10004},
                        /*speed=*/300, /*atk=*/5000, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 载体拍对手一次，返回对手掉血
int strike(ScenarioBattle& b, bool foe_drop) {
    b.start();
    BattleContext& ctx = b.ctx();
    if (foe_drop) {
        ctx.ability_levels[1][0] = -1;   // 对手攻击槽 -1 → 门开
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return 99999 - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    long long off_sum = 0;
    long long on_sum = 0;
    constexpr int kTrials = 3;
    for (int i = 0; i < kTrials; ++i) {
        ScenarioBattle b(make_party(carrier_pet()), make_party(plain_pet()));
        off_sum += strike(b, /*foe_drop=*/false);
    }
    for (int i = 0; i < kTrials; ++i) {
        ScenarioBattle b(make_party(carrier_pet()), make_party(plain_pet()));
        on_sum += strike(b, /*foe_drop=*/true);
    }
    const double avg_off = static_cast<double>(off_sum) / kTrials;
    const double avg_on = static_cast<double>(on_sum) / kTrials;
    const double ratio = avg_on / avg_off;
    // 门关基准：无降态 → 伤害 = 基硠（view 未加成）；门开 = ×(100+100)/100 = 2 倍
    CHECK(ratio >= 1.80 && ratio <= 2.20,
          "T3660 降态威力 +100% 平均比值越界：on=" << avg_on << " off=" << avg_off
                                                    << " ratio=" << ratio);
    std::cout << "  [T3660] 1953 降态门 +100%（off=" << avg_off << " on=" << avg_on
              << " ratio=" << ratio << "）通过\n";

    std::cout << "scenario_3660 PASS (J185 对手降态威力提升族)" << std::endl;
    return 0;
}
