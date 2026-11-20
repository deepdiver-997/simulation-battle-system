// 场景 4100 —— K254 对手害怕→烧伤族（299，soul_lib/k254_fear_burn.cpp）
//（2026-10-20，worktree ../agents/k254；号段 4100-4109 → T4100-T4103 四块）
//
// 断言口径（299「若对手处于害怕状态则烧伤」）：
//   - 门开：停泊 ROUND_START 前给对手预铺害怕 → 推进过 ROUND_START → 对手被烧伤
//     （has_active_abnormal_status(1, Burn=2)）。
//   - 门闭：对手无异常 → 不烧伤。
//   - 别的异常（非害怕）→ 门仍闭 → 不烧伤（条件是"害怕"不是"任意异常"）。
//   - 时长：烧伤持续 2 回合（random_anomaly_duration() inline 恒 2）。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击载体
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);  // 1

ElfPet mark_pet() {
    // 零参行（args 空）。
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(299, "K254 魂印", "",
                                               EffectArgs(std::vector<int>{})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4100 门开：对手害怕 → 烧伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        // 预铺害怕（等价"对手处于害怕状态"）。
        apply_anomaly(&ctx, 1, kFear, 2, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kFear));
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T4100 299：对手处于害怕状态 → 对手被烧伤");
        std::cout << "  [T4100] 门开烧伤 通过\n";
    }

    // ══ T4101 门闭：对手无异常 → 不烧伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(!ctx.has_active_abnormal_status(1, kBurn),
              "T4101 299：对手无异常 → 不烧伤（门闭对照）");
        std::cout << "  [T4101] 门闭对照 通过\n";
    }

    // ══ T4102 非害怕异常 → 门仍闭 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        // 预铺中毒（弱化类，非害怕）——门应仍闭。
        apply_anomaly(&ctx, 1, kPoison, 2, /*actor=*/0);
        REQUIRE(ctx.has_active_abnormal_status(1, kPoison));
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(!ctx.has_active_abnormal_status(1, kBurn),
              "T4102 299：对手中毒（非害怕）→ 不烧伤（门只认害怕）");
        std::cout << "  [T4102] 非害怕异常对照 通过\n";
    }

    // ══ T4103 时长：烧伤持续 2 回合 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        apply_anomaly(&ctx, 1, kFear, 2, /*actor=*/0);
        b.step_past(State::BATTLE_ROUND_START);
        REQUIRE(ctx.has_active_abnormal_status(1, kBurn));
        const int end_round = ctx.get_abnormal_status_end_round(1, kBurn);
        CHECK(end_round > ctx.roundCount,
              "T4103 299：烧伤有非零剩余回合（end_round > roundCount）");
        std::cout << "  [T4103] 烧伤时长 通过\n";
    }

    std::cout << "scenario_4100 PASS (K254 对手害怕→烧伤族)" << std::endl;
    return 0;
}
