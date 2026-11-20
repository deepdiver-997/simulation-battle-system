#include <utils/sbs_rand.h>
// 场景 3405 —— J181 先手吸取族（1362，moves_lib/j_first_drain.cpp）
//（2026-10-07，worktree ../agents/j181，看板段 3405-3414 → T3405-T3407）
//
// 断言口径（吸取=平点数直写，零掷点 → 精确断言）：
//   - T3405 陨霆雷刑 28489（drain=300，先手）：⚠️ 载体是属性技（category=4，无伤害
//     阶段）→ 对手失血恰 = 300（纯吸取）；持印者 hp 预置 50000 → 回合后恰 = 50300。
//   - T3406 同载体后手位（speed 翻转）：先手门短路 → hp 恰 = 50000（不吸取；
//     链上 1549 后手先制+2 与 681 必爆窗不影响 hp 断言）。
//   - T3407 丰岁百香祝 36493（drain=300，⚠️ 立单行误写 100，切片后移一位更正）：
//     先手 → hp 恰 = 50300；链上 1247 禁回血（无回血可禁）、1511 未接静默——均无扰。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMaxHp = 99999;

ElfPet holder_pet(int skill_id, int speed, int hp) {
    ElfPet p = make_pet(1, "印主", {skill_id, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/100, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3405 先手吸取 300（陨霆雷刑）══
    {
        ScenarioBattle b(make_party(holder_pet(28489, /*speed=*/300, kMaxHp)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(ctx.getPet(0).hp, 50300, "T3405 1362：先手吸取 300 精确回血");
        CHECK_EQ(foe_loss, 300,
                 "T3405 1362：属性技载体无伤害阶段 → 对手恰失吸取的 300");
        std::cout << "  [T3405] 先手吸取300 通过 (foe_loss=" << foe_loss << ")\n";
    }

    // ══ T3406 后手位：门短路不吸取 ══
    {
        ScenarioBattle b(make_party(holder_pet(28489, /*speed=*/100, 50000)),
                         make_party(plain_foe(/*speed=*/300)));
        sbs_srand(7);
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 50000, "T3406 1362：后手 → 不吸取（hp 不动）");
        std::cout << "  [T3406] 后手短路 通过\n";
    }

    // ══ T3407 丰岁百香祝（drain=300，切片更正记档）══
    {
        ScenarioBattle b(make_party(holder_pet(36493, /*speed=*/300, kMaxHp)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 50000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 50300, "T3407 1362：丰岁百香祝吸取 300 精确回血");
        std::cout << "  [T3407] 丰岁百香祝吸取300 通过\n";
    }

    std::cout << "scenario_3405 PASS (J181 先手吸取族)" << std::endl;
    return 0;
}
