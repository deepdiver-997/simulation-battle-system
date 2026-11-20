// 场景 5660 —— J235 药剂反噬族（69，moves_lib/j_potion_backlash.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5660-5669）
//
// 断言口径（药剂即时结算 + 绝对 hp 差）：
//   - 69（药剂反噬 20314 纯链 [69]|[5]）：{0}=5 回合窗。
//   - 药剂 300011 = 初级体力药剂（+20 HP，USE_MEDICINE 在操作提交时即时结算）。
//   - a) 窗内：对手 hp 100 → 嗑药变反噬 → 80（-20，而非 +20）。
//   - b) 对照：未施放 69 → 嗑药正常回复 → 120。
//   - c) 窗过期：R1 施放，R2-R8 挂机（roundCount 8-0 ≥ 5）→ R9 嗑药恢复为 120。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物 40（填充刀）
constexpr int kYaoJi   = 20314;   // [69]|[5]（纯链，cat4 属性技）
constexpr int kHpPotion = 300011; // 初级体力药剂 +20 HP
constexpr int kMaxHp   = 99999;
constexpr int kFoeHp   = 100;     // 药剂口径的可观测血量
constexpr int kPotionHeal = 20;

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kYaoJi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 0;
    }
    return p;
}

// 双方挂机一回合。
void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

// 对手嗑体力药剂（操作提交即结算）。
void foe_drink(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5660 69 对手嗑体力药剂 → 回复变扣除 ══
    {
        {   // (a) 窗内反噬
            const MedicineStock foe_meds{{kHpPotion, 9}};
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()), {}, foe_meds);
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = kFoeHp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            foe_drink(b);
            CHECK_EQ(ctx.getPet(1).hp, kFoeHp - kPotionHeal,
                     "T5660.a 69：窗内嗑药反噬 -20 → hp 80（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
        }
        {   // (b) 对照：未施放 → 正常回复
            const MedicineStock foe_meds{{kHpPotion, 9}};
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()), {}, foe_meds);
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = kFoeHp;
            idle_round(b);
            foe_drink(b);
            CHECK_EQ(ctx.getPet(1).hp, kFoeHp + kPotionHeal,
                     "T5660.b 69 对照：未施放 → 嗑药正常回复 +20 → hp 120（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
        }
        {   // (c) 窗过期：R2-R8 挂机后嗑药恢复
            const MedicineStock foe_meds{{kHpPotion, 9}};
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()), {}, foe_meds);
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int i = 0; i < 7; ++i) {   // R2..R8（roundCount 8-0 ≥ 5）
                idle_round(b);
            }
            ctx.getPet(1).hp = kFoeHp;
            foe_drink(b);
            CHECK_EQ(ctx.getPet(1).hp, kFoeHp + kPotionHeal,
                     "T5660.c 69 窗过期：R9 嗑药恢复正常 +20 → hp 120（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
        }
        std::cout << "  [T5660] 69 窗内反噬 / 未施放对照 / 窗过期 通过\n";
    }

    std::cout << "scenario_5660 PASS (J235 药剂反噬族)" << std::endl;
    return 0;
}
