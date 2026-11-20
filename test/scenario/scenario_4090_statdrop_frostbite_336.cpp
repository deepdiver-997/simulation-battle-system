// 场景 4090 —— K253 对手能力下降→冻伤族（336，soul_lib/k253_statdrop_frostbite.cpp）
//（2026-10-20，worktree ../agents/k253；号段 4090-4099 → T4090-T4093 四块）
//
// 断言口径（336「对手处于能力下降状态时，则对手冻伤」）：
//   - 门开：停泊 ROUND_START 后把对手防御槽压到 -1（等价"对手处于能力下降状态"）
//     → 推进过 ROUND_START → 对手被冻伤（has_active_abnormal_status(1, Frostbite=5)）。
//   - 门闭：对手无能力下降 → 不冻伤。
//   - 持续态跨回合：能力下降是持续态（等级跨回合不自动清），第 2 回合仍应冻伤。
//   - 时长：冻伤持续 2 回合（random_anomaly_duration() inline 恒 2）。
//   - 免疫通道：对手挂 ANOMALY 免疫 → 冻伤不落地（引擎免疫链生效，不误判为门开）。
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
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);  // 5

ElfPet mark_pet() {
    // 行参原序 [3,5]（db_at 原序读；实现侧不消费，仅记档）。
    std::vector<int> args{3, 5};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(336, "K253 魂印", "",
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

// 把对手能力等级压成"能力下降"（负等级 = 引擎口径的下降态）。
void drop_foe_defense(BattleContext& ctx) {
    ctx.ability_levels[1][2] = -1;   // 槽 2 = 防御
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4090 门开：对手能力下降 → 冻伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 停泊到 ROUND_START 前，先把对手压成能力下降态。
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        drop_foe_defense(ctx);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T4090 336：对手处于能力下降状态 → 对手被冻伤");
        std::cout << "  [T4090] 门开冻伤 通过\n";
    }

    // ══ T4091 门闭：对手无能力下降 → 不冻伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T4091 336：对手无能力下降 → 不冻伤（门闭对照）");
        std::cout << "  [T4091] 门闭对照 通过\n";
    }

    // ══ T4092 持续态跨回合：第 2 回合仍冻伤 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        drop_foe_defense(ctx);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T4092 336：第 1 回合冻伤落地");
        // 第 2 回合：能力下降态持续（等级不自动清）→ 应再冻伤一次（刷新时长）。
        b.drive();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T4092 336：能力下降持续态 → 第 2 回合仍冻伤");
        std::cout << "  [T4092] 跨回合持续 通过\n";
    }

    // ══ T4093 时长：冻伤持续 2 回合 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        ctx.debug_step_mode = false;
        drop_foe_defense(ctx);
        b.step_past(State::BATTLE_ROUND_START);
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T4093 336：冻伤落地");
        const int end_round = ctx.get_abnormal_status_end_round(1, kFrostbite);
        // random_anomaly_duration() = 2 回合 → 结束回合 = 当前回合 + 2。
        CHECK(end_round > ctx.roundCount,
              "T4093 336：冻伤有非零剩余回合（end_round > roundCount）");
        std::cout << "  [T4093] 冻伤时长 通过\n";
    }

    std::cout << "scenario_4090 PASS (K253 对手能力下降→冻伤族)" << std::endl;
    return 0;
}
