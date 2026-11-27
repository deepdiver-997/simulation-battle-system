#include <utils/sbs_rand.h>
// 场景 2822 —— K5 effect 1502（追击者M3 3911）：「每回合自身速度+1，自身处于速度
// 提升状态时闪避率提升50%」
//
// 实现：soul_lib/k_hit_dodge_aura.cpp —— ROUND_START stat_change(速槽4,+db[0]) +
// view_levels[速]>0 门 → ws.dodge_rate[self]=db[1]%。
//
// 断言口径（dodge_rate 减算 hit_chance = accuracy - dodge×100 → 极值确定性）：
//   A 速度爬坡：两回合 NONE → 速度等级 =2（stat_change 每回合 +1）。
//   B 槽位值：提速态 dodge_rate[0]==0.5。
//   C 极值行为：db[1]=100 → dodge 1.0 → acc=100 攻击恒 miss。
//   D 负例：db[0]=0（不提速）→ dodge 保持 0 → acc=100 恒命中 + 速度等级仍 0。
#include <cstdlib>
#include <iostream>
#include <memory>

#include <abnormal-system/abnormal-types.h>
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

ElfPet holder_pet(std::vector<int> db_args) {
    return scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                              make_mark(1502, std::move(db_args), 3911));
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

// 驱动 n 个双 NONE 回合（只跑时点桶，无伤害噪声），返回末次 drive 后的 ctx。
std::unique_ptr<ScenarioBattle> drive_none_rounds(std::vector<int> db_args, int rounds) {
    auto b = std::make_unique<ScenarioBattle>(scenario::make_party(holder_pet(std::move(db_args))),
                                              scenario::make_party(attacker_pet()));
    b->ctx().seed_rng(20261004);
    b->start();
    for (int r = 0; r < rounds; ++r) {
        b->submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b->submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b->drive();
    }
    return b;
}

int probe_loss(std::vector<int> db_args) {
    ScenarioBattle b(scenario::make_party(holder_pet(std::move(db_args))),
                     scenario::make_party(attacker_pet()));
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

    // A/B 速度爬坡 + 提速态闪避槽位
    {
        auto b = drive_none_rounds({1, 50, 1}, /*rounds=*/2);
        BattleContext& ctx = b->ctx();
        CHECK_EQ(ctx.ws.view_levels[0][kAbilityLevelIndexSpeed], 2,
                 "A 每回合速度+1（两回合后等级 2）");
        CHECK_EQ(ctx.ws.dodge_rate[0], 0.5f, "B 提速态 dodge_rate=0.5（真实行 db[1]=50）");
    }

    // C 极值：dodge 1.0 → 恒 miss
    CHECK_EQ(probe_loss({1, 100, 1}), 0, "C db[1]=100 → dodge 1.0 → acc=100 恒 miss");

    // D 负例：不提速 → 无闪避 → 恒命中
    CHECK(probe_loss({0, 50, 1}) > 0, "D db[0]=0 不提速 → dodge 保持 0 → acc=100 恒命中");
    {
        auto b = drive_none_rounds({0, 50, 1}, /*rounds=*/1);
        BattleContext& ctx = b->ctx();
        CHECK_EQ(ctx.ws.view_levels[0][kAbilityLevelIndexSpeed], 0,
                 "D2 db[0]=0 → 速度等级不变");
        CHECK_EQ(ctx.ws.dodge_rate[0], 0.0f, "D3 未提速 → dodge_rate 不写（保持 0）");
    }

    std::cout << "scenario_2822 PASS (K5 effect1502 每回合提速+提速态闪避50%)" << std::endl;
    return 0;
}
