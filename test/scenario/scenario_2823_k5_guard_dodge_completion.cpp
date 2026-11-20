#include <utils/sbs_rand.h>
// 场景 2823 —— K5 260/280 闪避子句补全（K1 记档 → S1-P0 闪避钩子解锁）
//
// 实现：soul_lib/k_guard_family.cpp soulmark_260/280_dodge_round_start —— ROUND_START
// 写 ws.dodge_rate（280 读 db[1]（2508=5、3077=0 不写）；260 monster 表（2425 鲍尔特
// =10、2424 鲍尔无闪避子句）。减伤部分 940 已断言，本场景只测闪避。
//
// 断言口径：
//   A 槽位值四例：280/2508=0.05、280/3077=0（db[1]=0）、260/2425=0.10、260/2424=0。
//   B 极值行为：280 db[1]=100 → dodge 1.0 → acc=100 非必中攻击恒 miss。
//   C 必中绕过：同印记下燕返（must_hit）照常命中——闪避是减算，accuracy_fix_100
//     跳过 dodge 减避（内核 skills.cpp 既有口径）；940 的必中探针回归安全依据。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;
constexpr int kYanFan = 10037;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet defender_pet(int effect_id, std::vector<int> db_args, int monster_id) {
    return scenario::make_pet(1, "守方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(effect_id, std::move(db_args), monster_id));
}
ElfPet attacker_pet(bool must_hit) {
    ElfPet p = scenario::make_pet(2, "攻方",
                                  {must_hit ? kYanFan : kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit ? 1 : 0;
        s.accuracy = 100;
    }
    return p;
}

float probe_dodge(int effect_id, std::vector<int> db_args, int monster_id) {
    ScenarioBattle b(scenario::make_party(defender_pet(effect_id, std::move(db_args), monster_id)),
                     scenario::make_party(attacker_pet(false)));
    sbs_srand(20261004);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return b.ctx().ws.dodge_rate[0];
}

int probe_loss(int effect_id, std::vector<int> db_args, int monster_id, bool must_hit) {
    ScenarioBattle b(scenario::make_party(defender_pet(effect_id, std::move(db_args), monster_id)),
                     scenario::make_party(attacker_pet(must_hit)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 槽位值四例
    CHECK_EQ(probe_dodge(280, {15, 5}, 2508), 0.05f, "A1 effect280 焱武圣尊 db[1]=5 → dodge 0.05");
    CHECK_EQ(probe_dodge(280, {30, 0}, 3077), 0.0f, "A2 effect280 神鹿仙灵 db[1]=0 → 不写（0）");
    CHECK_EQ(probe_dodge(260, {}, 2425), 0.10f, "A3 effect260 鲍尔特（monster 表）→ dodge 0.10");
    CHECK_EQ(probe_dodge(260, {}, 2424), 0.0f, "A4 effect260 鲍尔无闪避子句 → 0");

    // B 极值：dodge 1.0 → 非必中恒 miss
    CHECK_EQ(probe_loss(280, {15, 100}, 2508, false), 0,
             "B effect280 db[1]=100 → dodge 1.0 → acc=100 恒 miss");
    // C 必中绕闪避
    CHECK(probe_loss(280, {15, 100}, 2508, true) > 0,
          "C 必中技能绕 dodge_rate（accuracy_fix_100 跳过减避）");

    std::cout << "scenario_2823 PASS (K5 260/280 闪避子句补全)" << std::endl;
    return 0;
}
