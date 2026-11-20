#include <utils/sbs_rand.h>
// 场景 2820 —— K5 effect 7（傲世战魂 2553）：「使对手的命中率-6%」常驻光环
//
// 实现：soul_lib/k_hit_dodge_aura.cpp —— ROUND_START 写 ws.hit_rate_mod[opp]=0.94
//（全技命中率乘区，476 圣芒同款套路；args 空取行文字面）。
//
// 断言口径：
//   T1 槽位值：驱动 1 回合后 ws.hit_rate_mod[1]==0.94（ROUND_START 写、当回合存活）。
//   T2 截断归零：对手 accuracy=1 载体 ×0.94 → int 截断 = 0 → 恒 miss（确定性）。
//   T3 探针自证：无印记 + accuracy=100 → 命中（hp 掉），证 T2 的 miss 归因于乘区。
//   T4 必中绕过：燕返（must_hit）不吃命中率乘区（内核 accuracy 分支整段跳过）——
//     内核契约断言，钉住"光环不治必中"的既有口径。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;   // 拍打：攻击技载体（断言时手动关必中、定 accuracy）
constexpr int kYanFan = 10037;  // 燕返：威力 60 必中

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 守方（0 方，印记持有者，速度慢后手）；攻方（1 方）用 accuracy/必中可配的拍打。
ElfPet defender_pet(int id, SoulMark mark) {
    return scenario::make_pet(id, "守方", {10005, 10005, 10005, 10005, 10005},
                              /*speed=*/100, /*atk=*/10, /*hp=*/1000000, std::move(mark));
}
ElfPet attacker_pet(int accuracy, bool must_hit) {
    ElfPet p = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = must_hit ? 1 : 0;
        s.accuracy = must_hit ? 100 : accuracy;
    }
    return p;
}

// 一回合：攻方 SELECT_SKILL(0)、守方 NONE → 守方 hp 损失。
int probe_loss(SoulMark mark, int accuracy, bool must_hit) {
    ScenarioBattle b(scenario::make_party(defender_pet(1, std::move(mark))),
                     scenario::make_party(attacker_pet(accuracy, must_hit)));
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

    // T1 槽位值
    {
        ScenarioBattle b(scenario::make_party(defender_pet(1, make_mark(7, {}, 2553))),
                         scenario::make_party(attacker_pet(100, false)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ws.hit_rate_mod[1], 0.94f,
                 "T1 ROUND_START 写 hit_rate_mod[opp]=0.94（-6%）");
    }

    // T2 accuracy=1 ×0.94 → int 截断 0 → 恒 miss
    CHECK_EQ(probe_loss(make_mark(7, {}, 2553), /*accuracy=*/1, false), 0,
             "T2 accuracy=1 载体 ×0.94 截断归零 → 恒 miss");
    // T3 探针自证：无印记 acc=100 → 命中
    CHECK(probe_loss(SoulMark{}, 100, false) > 0, "T3 无印记 acc=100 → 命中（探针自证）");
    // T4 必中绕过命中率乘区
    CHECK(probe_loss(make_mark(7, {}, 2553), 100, true) > 0,
          "T4 必中技能不吃 hit_rate_mod（内核 accuracy 分支跳过）");

    std::cout << "scenario_2820 PASS (K5 effect7 对手命中率-6% 常驻光环)" << std::endl;
    return 0;
}
