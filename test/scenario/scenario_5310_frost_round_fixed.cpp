// K352 / 1723：5310-5319已alloc。停泊ROUND_END隔离异常自身的自然扣血。
#include <algorithm>
#include <iostream>
#include <vector>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include "scenario_harness.h"
using namespace scenario;
namespace {
ElfPet pet(bool marked, std::vector<int> args) {
    SoulMark sm(1723, "K·棱镜", "", EffectArgs(std::move(args)));
    sm.monster_id = 4133;
    return make_pet(4133, "固伤探针", {10001,10001,10001,10001,10001},
                    200, 1, 5000, marked ? sm : SoulMark());
}
void run_case(const char* label, std::vector<int> args, int foe_status,
              int expected_damage, int hood = 0, bool immune = false,
              int owner = 0, bool boss = false, bool self_frost = false,
              int rounds = 1, bool marked = true) {
    ScenarioBattle b(make_party(pet(marked && owner == 0, args)),
                     make_party(pet(marked && owner == 1, args)));
    b.ctx().is_boss_challenge = boss;
    b.start();
    auto& ctx = b.ctx();
    const int enemy = 1 - owner;
    for (int r = 0; r < rounds; ++r) {
        ctx.debug_step_mode = true;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        ctx.debug_step_mode = false;
        b.step_until(State::BATTLE_ROUND_END);
        if (foe_status >= 0) {
            ctx.set_abnormal_status_end_round(enemy, foe_status, ctx.roundCount + 10);
            CHECK(ctx.has_active_abnormal_status(enemy, foe_status), "异常前置已生效");
        }
        if (self_frost) ctx.set_abnormal_status_end_round(owner, 5, ctx.roundCount + 10);
        auto& foe = ctx.getPet(enemy);
        foe.shield_bank_.clear();
        foe.shield_bank_.add(0, 500, 997231, 0);
        foe.hood_bank_.clear();
        if (hood) foe.hood_bank_.add(0, hood, 997232, 0);
        if (immune) ctx.grant_immunity(enemy, ImmunityType::PINK_DAMAGE, coverage_all(), 0, 0, 997233);
        const int before = foe.hp;
        const int own_before = ctx.getPet(owner).hp;
        b.step_past(State::BATTLE_ROUND_END);
        const int landed = immune ? 0 : std::max(0, expected_damage - hood);
        CHECK_EQ(before - foe.hp, landed, "回合末固定伤害的精确前后HP差");
        CHECK_EQ(foe.shield_bank_.total(), 500, "FIXED粉伤不消耗红伤护盾");
        CHECK_EQ(foe.hood_bank_.total(), immune ? hood : std::max(0, hood - expected_damage),
                 "粉伤吃护罩，免粉时不消耗护罩");
        CHECK_EQ(ctx.getPet(owner).hp, own_before, "不扣印主体力");
        std::cout << label << " R" << r+1 << " loss=" << before - foe.hp
                  << " hood=" << foe.hood_bank_.total() << '\n';
        b.drive();
    }
}
}  // namespace
int main() {
    if (!PetFactory::initialize_runtime_data()) return 1;
    run_case("normal", {200,0}, -1, 200, 0, false, 0, false, false, 2);
    run_case("foe-frost", {200,0}, 5, 400, 0, false, 0, false, false, 2);
    run_case("burn-is-not-frost", {200,0}, 2, 200);
    run_case("self-frost-does-not-double", {200,0}, -1, 200, 0, false, 0, false, true);
    run_case("raw-db-173", {173,0}, 5, 346);
    run_case("explicit-zero", {0,0}, 5, 0);
    run_case("missing-args", {}, -1, 200);
    run_case("owner1-boss", {200,0}, 5, 400, 0, false, 1, true);
    run_case("hood-normal", {200,0}, -1, 200, 250);
    run_case("hood-double", {200,0}, 5, 400, 250);
    run_case("pink-immune", {200,0}, 5, 400, 250, true);
    run_case("no-mark", {200,0}, 5, 0, 0, false, 0, false, false, 1, false);
    std::cout << "scenario_5310 PASS (K352 1723 冻伤门回合末固伤)\n";
    return 0;
}
