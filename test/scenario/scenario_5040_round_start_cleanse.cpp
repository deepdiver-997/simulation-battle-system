// K345 / 2433：5040-5049 已由 taskctl alloc 分配。
// 停泊 ROUND_START 前后观测异常槽，避免自然到期/回合末扣血造成假绿。
#include <array>
#include <iostream>
#include <vector>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include "scenario_harness.h"

using namespace scenario;
namespace {
constexpr std::array<int, 3> kStatuses{2, 7, 19};  // 烧伤、疲惫、瘫痪

ElfPet pet(bool marked) {
    SoulMark mark(2433, "火箭骑士君", "", EffectArgs(std::vector<int>{1, 0}));
    mark.monster_id = 4815;
    return make_pet(4815, "清异常探针", {10001, 10001, 10001, 10001, 10001},
                    200, 1, 5000, marked ? mark : SoulMark());
}

void begin_round(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
}

void check_round(bool marked, bool boss, int owner, int rounds) {
    ScenarioBattle b(make_party(pet(marked && owner == 0)),
                     make_party(pet(marked && owner == 1)));
    b.ctx().is_boss_challenge = boss;
    b.start();
    auto& ctx = b.ctx();
    for (int r = 0; r < rounds; ++r) {
        begin_round(b);
        std::array<std::array<int, 3>, 2> before{};
        for (int side = 0; side < 2; ++side) {
            for (size_t i = 0; i < kStatuses.size(); ++i) {
                ctx.set_abnormal_status_end_round(side, kStatuses[i], ctx.roundCount + 10);
                before[side][i] = ctx.get_abnormal_status_end_round(side, kStatuses[i]);
                CHECK(ctx.has_active_abnormal_status(side, kStatuses[i]), "前置：异常确实生效");
            }
        }
        const int hp_before = ctx.getPet(owner).hp;
        b.step_past(State::BATTLE_ROUND_START);
        for (int side = 0; side < 2; ++side) {
            const bool cleared = marked && !boss && side == owner;
            for (size_t i = 0; i < kStatuses.size(); ++i) {
                const int after = ctx.get_abnormal_status_end_round(side, kStatuses[i]);
                CHECK_EQ(after, cleared ? 0 : before[side][i],
                         "2433：逐异常槽前后快照，只有非boss印主被解除");
            }
        }
        CHECK_EQ(ctx.getPet(owner).hp, hp_before, "效果解除不触发异常到期伤害");
        std::cout << "2433 owner=" << owner << " marked=" << marked << " boss=" << boss
                  << " round=" << r + 1 << " before=" << before[owner][0]
                  << " after=" << ctx.get_abnormal_status_end_round(owner, kStatuses[0]) << '\n';
        // 在本回合首之后新加异常，必须保留至下回合首，不能误写成回合末清除。
        ctx.set_abnormal_status_end_round(owner, 2, ctx.roundCount + 10);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(owner, 2), "回合首之后新加的异常留到下回合");
    }
}
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    check_round(true, false, 0, 3);   // 每回合重注册，不能只执行一次
    check_round(true, false, 1, 2);   // owner 1 同样生效
    check_round(true, true, 0, 1);    // boss_invalid 注册面生效
    check_round(false, false, 0, 1);  // 无印对照，不是内核自然清除
    std::cout << "scenario_5040 PASS (K345 2433 回合开始解除异常)\n";
    return 0;
}
