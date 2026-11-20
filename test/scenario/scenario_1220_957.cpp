// 场景 1220 —— J37 未击败条件族之 957（moves_lib/j957_all_down_cond_up.cpp）
//（2026-10-02，worktree ../agents/j37；号段 1220-1229 → T1220 单块）
//
// ⚠️ 本单 964/920 与 feat/j12 重复立项（j_defeat_gate2.cpp 已实现并在 j12 分支
//   回归绿），不重复注册——本场景只覆盖新产码 957。
//
// 断言口径：
//   - 957 用 ability_levels 精确断言：干净对手 → 对手六槽全 -1、自身全 0；
//     烧伤对手 → 对手六槽全 -1 且自身六槽全 +1；
//   - 载体：鬼鬼祟祟 28254 [957]|[1,2,1] 单效果载体（属性技，无红伤噪声）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kGuiGui = 28254;  // 鬼鬼祟祟[957]|[1,2,1]：属（对手全属性-1，烧伤→自身+1）
constexpr int kPaiDa = 10004;   // 拍打：物40（占位探针技）
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

bool six_slots_all_eq(BattleContext& ctx, int side, int expected) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] != expected) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1220 957 对手全属性-1，烧伤对手→自身额外全属性+1（鬼鬼祟祟）══
    {
        {
            ScenarioBattle b(
                make_party(self_pet({kGuiGui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(six_slots_all_eq(ctx, 1, -1), "T1220.a 957：干净对手六槽全 -1");
            CHECK(six_slots_all_eq(ctx, 0, 0), "T1220.a 957：对手无烧伤 → 自身不动");
        }
        {
            ScenarioBattle b(
                make_party(self_pet({kGuiGui, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, kBurn, /*duration_rounds=*/9, /*actor=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(six_slots_all_eq(ctx, 1, -1), "T1220.b 957：烧伤对手六槽全 -1");
            CHECK(six_slots_all_eq(ctx, 0, 1), "T1220.b 957：烧伤 → 自身六槽全 +1");
        }
        std::cout << "  [T1220] 957 全属性降+条件反升 通过\n";
    }

    std::cout << "scenario_1220 PASS (J37 957 全属性条件升降)" << std::endl;
    return 0;
}
