#include <utils/sbs_rand.h>
// 场景 2862 —— K9 effect 134：草系技能威力 +5%（灵驹族 809/810/811）
//
// 断言口径：**字段断言**（停泊到 FIRST_SKILL_EFFECT 后读威力视图）——伤害比例断言
// 不可用：rewritten 重算重掷 85%~100% 浮点（首跑实证 2198 vs 预期 2084）。
//   A 草系载体（疾风刃 10003 type_id=1）：view == 基准 ×105/100。
//   B 非草载体（拍打 10004）：view == 基准（零行为）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kJiFengRen = 10003;   // 疾风刃：草系物理 威力55（type_id=1）
constexpr int kPaiDa = 10004;       // 拍打：非草对照

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet(int skill_id, bool with_mark) {
    ElfPet p = scenario::make_pet(1, "持有方", {skill_id, 10005, 10005, 10005, 10005},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000,
                                  with_mark ? make_mark(134, {}, 809) : SoulMark{});
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}
ElfPet dummy_pet() {
    return scenario::make_pet(2, "木桩", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

int probe_power_view(int skill_id, bool with_mark) {
    ScenarioBattle b(scenario::make_party(holder_pet(skill_id, with_mark)),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    ctx.debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    ctx.debug_step_mode = false;
    b.step_past(State::BATTLE_FIRST_SKILL_EFFECT);
    const int view = ctx.ws.skill_power_view[0].value;
    b.drive();
    return view;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 草系载体
    {
        const int base = probe_power_view(kJiFengRen, false);
        CHECK(base > 0, "草系基准视图物化");
        CHECK_EQ(probe_power_view(kJiFengRen, true), base * 105 / 100, "A 草系威力 +5%（精确）");
    }
    // B 非草对照
    {
        const int base = probe_power_view(kPaiDa, false);
        CHECK_EQ(probe_power_view(kPaiDa, true), base, "B 非草系零行为");
    }

    std::cout << "scenario_2862 PASS (K9 effect134 草系威力+5%·字段断言)" << std::endl;
    return 0;
}
