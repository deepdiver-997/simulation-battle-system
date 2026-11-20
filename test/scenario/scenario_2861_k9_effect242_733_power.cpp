#include <utils/sbs_rand.h>
// 场景 2861 —— K9 effect 242（对手<1/3 威力翻倍）+ 733（概率威力翻倍）
//
// 断言口径：**字段断言**——debug_step 停泊到 FIRST_SKILL_EFFECT 跑完后读
// ws.skill_power_view[0].value（威力视图已物化、本族节点已跑）。不走伤害比例断言：
// rewritten 重算会重掷 85%~100% 浮点（2862 首跑实证），伤害量不可精确对齐。
//
//   A 242（莫迪尔 2204，args 空 ×2）：木桩满血 → view=基准；压到 <1/3 → view=基准×2。
//   B 733（青莲剑神 3103）：[100,2] → view=基准×2（roll 100 在 roll_percent 短路，
//     不耗 rand）；[0,2] → view=基准。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;
constexpr int kPaiDaPower = 10004 == kPaiDa ? 0 : 0;   // 占位（基准 view 运行时读）

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet attacker_pet(int effect_id, std::vector<int> db_args, int monster_id) {
    ElfPet p = scenario::make_pet(1, "进攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000,
                                  make_mark(effect_id, std::move(db_args), monster_id));
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

// 停泊到 FIRST_SKILL_EFFECT 跑完，读进攻方威力视图。
int probe_power_view(int effect_id, std::vector<int> db_args, int monster_id,
                     int force_dummy_hp) {
    ScenarioBattle b(scenario::make_party(attacker_pet(effect_id, std::move(db_args), monster_id)),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    if (force_dummy_hp > 0) {
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = force_dummy_hp;
    }
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

    // A 242
    {
        const int v_full = probe_power_view(242, {}, 2204, 0);
        CHECK(v_full > 0, "满血时威力视图已物化");
        const int v_low = probe_power_view(242, {}, 2204, /*force_dummy_hp=*/100000);
        CHECK_EQ(v_low, v_full * 2, "A 木桩 10万 < 1/3（≈33万）→ 视图 ×2");
    }

    // B 733
    {
        const int v_off = probe_power_view(733, {0, 2}, 3103, 0);
        CHECK(v_off > 0, "733 基准视图物化");
        CHECK_EQ(probe_power_view(733, {100, 2}, 3103, 0), v_off * 2, "B1 [100,2] → 视图 ×2");
        CHECK_EQ(probe_power_view(733, {50, 2}, 3103, 0) > 0, true,
                 "B2 [50,2] 真实概率档不炸");
    }

    std::cout << "scenario_2861 PASS (K9 effect242/733 条件威力翻倍·字段断言)" << std::endl;
    return 0;
}
