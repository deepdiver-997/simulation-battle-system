#include <utils/sbs_rand.h>
// 场景 2870 —— K10 effect 637：每回合回 150HP + 反弹 1/3 对手伤害
//
// 断言口径（srand 对齐 → 整数精确；反弹走 K3 thorn 同款 watcher，交付时点由整回合
// 量测兜住——950 同款）：
//   A 回血：持有方压到 500 体 → ROUND_START +150 → 650（双方 NONE，无噪声）。
//   B 反弹：攻方打持有方 X 点 → 攻方掉 X/3（无印记对照=0）。
//   C 2905 行（denom=0）无反弹子句 → 攻方零损失。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet holder_pet(std::vector<int> db_args, int monster_id) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(637, std::move(db_args), monster_id));
}
ElfPet attacker_pet() {
    ElfPet p = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int probe_holder_heal(std::vector<int> db_args, int monster_id, int hp_start) {
    ScenarioBattle b(scenario::make_party(holder_pet(std::move(db_args), monster_id)),
                     scenario::make_party(attacker_pet()));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
    b.start();
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = hp_start;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
}

// 攻方打持有方，返回（持有方损血, 攻方损血）。
std::pair<int, int> probe_reflect(std::vector<int> db_args, int monster_id) {
    ScenarioBattle b(scenario::make_party(holder_pet(std::move(db_args), monster_id)),
                     scenario::make_party(attacker_pet()));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
    b.start();
    const int holder_hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    const int atk_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int holder_loss = holder_hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
    const int atk_loss = atk_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    return {holder_loss, atk_loss};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 回血 150
    CHECK_EQ(probe_holder_heal({150, 3, 1}, 2892, 500), 650, "A 压 500 体 → ROUND_START 回 150 → 650");
    // B 反弹 1/3
    {
        const auto [loss, refl] = probe_reflect({150, 3, 1}, 2892);
        CHECK(loss > 1000, "持有方受伤量级合理");
        CHECK_EQ(refl, loss / 3, "B 攻方被反弹 1/3（整数截断精确）");
    }
    // C 无反弹子句行
    {
        const auto [loss, refl] = probe_reflect({150, 0, 1}, 2905);
        CHECK(loss > 1000, "2905 行持有方受伤量级合理");
        CHECK_EQ(refl, 0, "C 2905 行（denom=0）无反弹");
    }

    std::cout << "scenario_2870 PASS (K10 effect637 回合回血+比例反弹)" << std::endl;
    return 0;
}
