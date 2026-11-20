#include <utils/sbs_rand.h>
// 场景 3445 —— K205 满血增伤族（325，soul_lib/k205_fullhp_power.cpp）
//（2026-10-07，worktree ../agents/k205；号段 3445-3454 → T3445-T3446 两块）
//
// 断言口径（2861/733 字段断言范式）：停泊 FIRST_SKILL_EFFECT 读
// ws.skill_power_view[0].value。基线 = 撞击威力 35；满血 → ×110/100=38；
// 掉血后 → 35（门闭）。属性技 → rewritten 不置位。
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
constexpr int kSuoTou = 20001;

ElfPet mark_pet(int hp, bool attr_skill = false) {
    std::vector<int> args{10, 0};
    const int skill0 = attr_skill ? kSuoTou : kJi;
    ElfPet p = make_pet(1, "印主", {skill0, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, hp,
                        /*soul_mark=*/SoulMark(325, "K205 魂印", "",
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

// 停泊读印主威力视图 + rewritten 旗（K187 范式）；force_hp>0 = 开局压血（非满血档）。
std::pair<int, bool> probe(bool attr_skill = false, int force_hp = 0) {
    ScenarioBattle b(make_party(mark_pet(99999, attr_skill)), make_party(plain_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    if (force_hp > 0) {
        ctx.seerRobot[0].elfPets[0].hp = force_hp;
    }
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    std::pair<int, bool> r{ctx.ws.skill_power_view[0].value,
                           ctx.ws.skill_power_view[0].rewritten};
    b.drive();
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3445 满血 → 威力 ×110/100（35 → 38）══
    {
        auto [view, rewritten] = probe();
        CHECK_EQ(view, 35 * 110 / 100, "T3445 325：满血 → 威力视图 +10%（逐位）");
        CHECK(rewritten, "T3445 325：rewritten 置位");
        std::cout << "  [T3445] 满血增伤 通过\n";
    }

    // ══ T3446 门闭：属性技 / 非满血 → 不写视图 ══
    {
        auto [view, rewritten] = probe(/*attr_skill=*/true);
        CHECK(!rewritten, "T3446 325：属性技 → 节点未写视图（类型门）");
        auto [view2, rewritten2] = probe(/*attr_skill=*/false, /*force_hp=*/50000);
        CHECK_EQ(view2, 35, "T3446 325：掉血 → 门闭原威力");
        CHECK(!rewritten2, "T3446 325：掉血 → rewritten 不置位（满血门）");
        std::cout << "  [T3446] 门闭对照 通过\n";
    }

    std::cout << "scenario_3445 PASS (K205 满血增伤族)" << std::endl;
    return 0;
}
