#include <utils/sbs_rand.h>
// 场景 500：E5 暴击触发族（496/553）
// （2026-09-29，E5；实现 resources/moves_lib/e_crit_trigger.cpp）
//
// T1 496 暴击回满：必暴载体（战神狂龙破 [496]）→ 残血用后回满（扣掉对手 1 点反击）。
// T1b 负例：不暴击 → 不回血。
// T2 553 暴击施加：必暴载体（半月斩 [100,6]=100% 害怕）→ 对手陷入害怕。
// T2b 负例：不暴击 → 不施加。
//
// 暴击用 set_crit_sixteenths(pet, 16) 确定性打开（harness 默认清零 crit_rate）；
// 553 概率 100% 走 roll_percent 短路，无 RNG 消费。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10001;   // 撞击
constexpr int kCarrier496 = 14427;   // 战神狂龙破 [496] power160 物理
constexpr int kCarrier553 = 15308;   // 半月斩 [553][100,6] power80 物理

ElfPet crit_carrier(int carrier, int crit_sixteenths) {
    ElfPet p = scenario::make_pet(1, "暴击方", {carrier, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, /*atk=*/20, 1000000);
    scenario::set_crit_sixteenths(p, crit_sixteenths);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    // atk=0：拍打反击恒 1 点（T1 断言里作可扣除的已知量）。
    ElfPet p = scenario::make_pet(2, "守方", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/0, 1000000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    sbs_srand(20260940);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    constexpr int kMax = 1000000;

    //========== T1 496：暴击 → 回满 ==========
    {
        sbs_srand(20260941);
        ScenarioBattle b(scenario::make_party(crit_carrier(kCarrier496, /*crit=*/16)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;   // 残血开局
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw1 = ctx.ws.raw_attack_damage[1];   // 守方反击裸伤（atk=0 → 1）
        CHECK(ctx.crit_happened[0], "T1 前置：本回合打出了致命一击");
        CHECK_EQ(ctx.getPet(0).hp, kMax - raw1, "T1 核心：暴击后回满（仅剩守方 1 点反击）");
        std::cout << "  [T1] 496 暴击回满 通过\n";
    }
    //========== T1b 496 负例：不暴击 → 不回血 ==========
    {
        sbs_srand(20260942);
        ScenarioBattle b(scenario::make_party(crit_carrier(kCarrier496, /*crit=*/0)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw1 = ctx.ws.raw_attack_damage[1];
        CHECK(!ctx.crit_happened[0], "T1b 前置：未暴击");
        CHECK_EQ(ctx.getPet(0).hp, 5000 - raw1, "T1b 核心：不暴击不回血（只剩守方反击扣血）");
        std::cout << "  [T1b] 496 不暴击不回血 通过\n";
    }

    //========== T2 553：暴击 → 100% 害怕 ==========
    {
        sbs_srand(20260943);
        ScenarioBattle b(scenario::make_party(crit_carrier(kCarrier553, /*crit=*/16)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.crit_happened[0], "T2 前置：本回合打出了致命一击");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T2 核心：暴击 → 对手 100% 害怕落地");
        std::cout << "  [T2] 553 暴击施加害怕 通过\n";
    }
    //========== T2b 553 负例：不暴击 → 不施加 ==========
    {
        sbs_srand(20260944);
        ScenarioBattle b(scenario::make_party(crit_carrier(kCarrier553, /*crit=*/0)),
                         scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.crit_happened[0], "T2b 前置：未暴击");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Fear)),
              "T2b 核心：不暴击不施加");
        std::cout << "  [T2b] 553 不暴击不施加 通过\n";
    }

    std::cout << "scenario_500 all passed" << std::endl;
    return 0;
}
