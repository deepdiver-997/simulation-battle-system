// 场景 4110 —— K255 对手异常→速度+1族（320，soul_lib/k255_opp_anomaly_speed.cpp）
//（2026-10-21，worktree ../agents/k255；号段 4110-4119 → T4110-T4113 四块）
//
// 断言口径（320「对手处于异常状态时则自身速度+1」）：
//   - 门开：停泊 ROUND_START 前给对手预铺异常 → 推进过 ROUND_START → 自身速度等级 +1
//     （ability_levels[0][4] 由 0 变 1）。
//   - 门闭：对手无异常 → 速度等级不变。
//   - 任意异常都算门开：预铺**控制类**异常（害怕）也应触发——本单门是"任意异常"，
//     与 336（能力下降）/299（害怕）那两单"特定码门"不同。
//   - 只加速度、不动其它槽：攻击/特攻/防御/特防/命中等级全部不变。
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
constexpr int kSpeedSlot = 4;  // 能力等级槽 4 = 速度
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2（弱化类）
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6（控制类）

ElfPet mark_pet() {
    // 行参原序 [2,1,1,4,1]（实现侧不消费，仅记档）。
    std::vector<int> args{2, 1, 1, 4, 1};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(320, "K255 魂印", "",
                                               EffectArgs(std::move(args))));
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

    // ══ T4110 门开：对手有异常 → 自身速度+1 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        REQUIRE(ctx.ability_levels[0][kSpeedSlot] == 0);
        // 预铺异常（等价"对手处于异常状态"）。
        apply_anomaly(&ctx, 1, kBurn, 2, /*actor=*/0);
        REQUIRE(ctx.has_any_active_abnormal_status(1));
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 1,
                 "T4110 320：对手处于异常状态 → 自身速度等级 +1");
        std::cout << "  [T4110] 门开加速 通过\n";
    }

    // ══ T4111 门闭：对手无异常 → 不加速 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "T4111 320：对手无异常 → 自身速度等级不变（门闭对照）");
        std::cout << "  [T4111] 门闭对照 通过\n";
    }

    // ══ T4112 控制类异常也开门（门是"任意异常"） ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        apply_anomaly(&ctx, 1, kFear, 2, /*actor=*/0);
        REQUIRE(ctx.has_any_active_abnormal_status(1));
        b.step_past(State::BATTLE_ROUND_START);
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 1,
                 "T4112 320：对手处于控制类异常（害怕）→ 自身速度等级 +1");
        std::cout << "  [T4112] 控制类异常对照 通过\n";
    }

    // ══ T4113 只加速度槽，其余槽不动 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        apply_anomaly(&ctx, 1, kBurn, 2, /*actor=*/0);
        b.step_past(State::BATTLE_ROUND_START);
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (i == kSpeedSlot) {
                continue;
            }
            CHECK_EQ(ctx.ability_levels[0][i], 0,
                     "T4113 320：非速度槽等级不变（槽位隔离）");
        }
        std::cout << "  [T4113] 槽位隔离 通过\n";
    }

    std::cout << "scenario_4110 PASS (K255 对手异常→速度+1族)" << std::endl;
    return 0;
}
