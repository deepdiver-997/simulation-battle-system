#include <utils/sbs_rand.h>
// 场景 2890 —— K166 effect 284（登场对手命中-1）+ 282（先制伤害减）
//
// 断言口径：
//   A 284：开场后对手命中等级（本体）== -1（stat_drop 槽 5）。
//   B 282：先制载体（涤罪 33889 pri=4）打持有方 → -20%（db[0] 整数精确，940 同款）；
//     非先制载体（拍打）→ 原伤（门关）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;
constexpr int kDiZui = 33889;   // 涤罪：先制 pri=4 特攻威力 20

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// A：284 探针
int probe_284() {
    ScenarioBattle b(
        scenario::make_party({scenario::make_pet(1, "持有方",
                                                 {10005, 10005, 10005, 10005, 10005}, 100, 10,
                                                 1000000, make_mark(284, {}, 2522))}),
        scenario::make_party(scenario::make_pet(2, "对手", {10005, 10005, 10005, 10005, 10005},
                                                100, 10, 1000000)));
    b.start();
    return b.ctx().ability_levels[1][kAbilityLevelIndexHit];
}

// B：282 探针（攻方 skill 可选先制/非先制）
int probe_282(int skill_id, std::vector<int> db_args) {
    ElfPet atk = scenario::make_pet(2, "攻方", {skill_id, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    for (Skills& s : atk.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    ScenarioBattle b(
        scenario::make_party({scenario::make_pet(1, "守方",
                                                 {10005, 10005, 10005, 10005, 10005}, 100, 10,
                                                 1000000, make_mark(282, std::move(db_args), 2516))}),
        scenario::make_party(atk));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
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

    // A 284 登场命中-1
    CHECK_EQ(probe_284(), -1, "A 登场 → 对手命中等级 -1（本体）");

    // B 282 先制伤害减 20%
    {
        const int base = probe_282(kDiZui, {0, 0});
        CHECK(base > 100, "先制载体基线量级合理");
        CHECK_EQ(probe_282(kDiZui, {20, 0}), base * 80 / 100, "B1 先制技 → -20%（精确）");
        const int pai_base = probe_282(kPaiDa, {0, 0});
        CHECK_EQ(probe_282(kPaiDa, {20, 0}), pai_base, "B2 非先制技 → 门关（原伤）");
    }

    std::cout << "scenario_2890 PASS (K16 6 effect284/282)" << std::endl;
    return 0;
}
