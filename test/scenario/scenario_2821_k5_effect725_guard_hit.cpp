#include <utils/sbs_rand.h>
// 场景 2821 —— K5 effect 725（焱武·圣尊 3043）：「受到的伤害减少50%,对方所有技能
// 命中率降低50%」
//
// 实现：soul_lib/k_hit_dodge_aura.cpp —— ENTER_EXIT arm 红伤 REDUCE_PCT（K1 3/280
// 同构）+ ROUND_START 写 ws.hit_rate_mod[opp]（"所有技能"= 全技乘区，非 attack 门控）。
//
// 断言口径：
//   A 减伤：必中燕返探针（攻防面板量级合理），srand 对齐 rand 序 → 减伤后损失
//     = 基线 × 50% 整数精确（940 同款）。
//   B 槽位值：真实行 db[50,50] → ws.hit_rate_mod[1]==0.5。
//   C 极值行为：db[1]=100 场景桩 → 乘 0 → acc=100 攻击恒 miss（确定性）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（减伤探针用，绕命中率乘区）
constexpr int kPaiDa = 10004;    // 拍打：命中率行为探针用

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet attacker_pet(int id, bool must_hit) {
    ElfPet p = scenario::make_pet(id, "进攻方",
                                  {must_hit ? kYanFan : kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit ? 1 : 0;
        s.accuracy = 100;
    }
    return p;
}
ElfPet defender_pet(int id, SoulMark mark) {
    return scenario::make_pet(id, "守方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000, std::move(mark));
}

int probe_loss(const SoulMark& mark, bool must_hit_attacker) {
    ScenarioBattle b(scenario::make_party(defender_pet(1, mark)),
                     scenario::make_party(attacker_pet(2, must_hit_attacker)));
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

    // A 减伤 50%（必中探针 + rand 对齐）
    const int base = probe_loss(SoulMark{}, true);
    CHECK(base > 1000, "基线伤害量级合理（atk=5000 vs def=100）");
    CHECK_EQ(probe_loss(make_mark(725, {50, 50}, 3043), true), base / 2,
             "A effect725 红伤减 50%（整数精确，940 同款）");

    // B/C 命中率乘区
    {
        ScenarioBattle b(scenario::make_party(defender_pet(1, make_mark(725, {50, 50}, 3043))),
                         scenario::make_party(attacker_pet(2, false)));
        BattleContext& ctx = b.ctx();
        ctx.seed_rng(20261004);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ws.hit_rate_mod[1], 0.5f, "B 真实行 db[1]=50 → hit_rate_mod[opp]=0.5");
    }
    CHECK_EQ(probe_loss(make_mark(725, {50, 100}, 3043), false), 0,
             "C db[1]=100 → 全技命中率乘 0 → acc=100 恒 miss");

    std::cout << "scenario_2821 PASS (K5 effect725 减伤50%+对手全技命中-50%)" << std::endl;
    return 0;
}
