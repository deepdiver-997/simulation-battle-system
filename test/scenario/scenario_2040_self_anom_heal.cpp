// 场景 2040 —— J110 自施加/异常回满族（2 宏，moves_lib/j_self_anom_heal.cpp）
//（2026-10-03，worktree ../agents/j110；号段 2040-2049 → T2040-T2041 两块）
//
// 断言口径：
//   - 2196（火箭头槌 [100,7] 物150 纯链）：chance=100 → 自身疲惫(7) 确定性落地；
//   - 770（火灵重生咒 [770] 物150 纯链）：对手预置中毒 → 载体命中 → 回满 5000 整；
//     对照 = 对手无异常 → 不回。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kHuoJian = 37587; // 火箭头槌 [2196]|[100,7]：物150（自施加载体）
constexpr int kHuoLing = 17657; // 火灵重生咒 [770]：物150（异常回满载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2040 2196 自施加疲惫（火箭头槌 [100,7]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kHuoJian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(0, 7),
              "T2040.a 2196：chance=100 → 自身疲惫(7) 落地");
        std::cout << "  [T2040] 2196 自施加 通过\n";
    }

    // ══ T2041 770 对手异常 → 回满（火灵重生咒 [770]）══
    {
        // a) 对手中毒 → 回满
        {
            ScenarioBattle b(
                make_party(mk_self({kHuoLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;                  // 预置缺口
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Poison),
                                /*duration=*/5, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T2041.a 770：对手异常 → 回满（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 对照：对手无异常 → 不回
        {
            ScenarioBattle b(
                make_party(mk_self({kHuoLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 3000,
                     "T2041.b 770 对照：对手无异常不回（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T2041] 770 异常回满 通过\n";
    }

    std::cout << "scenario_2040 PASS (J110 自施加/异常回满族 2196/770)" << std::endl;
    return 0;
}
