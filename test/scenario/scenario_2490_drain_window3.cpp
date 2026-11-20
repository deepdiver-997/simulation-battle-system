// 场景 2490 —— J158 吸取窗族三（796，moves_lib/j_drain_window3.cpp）
//（2026-10-04，worktree ../agents/j158；号段 2490-2499 → T2490 两块）
//
// 断言口径：纯链载体 碎裂晶胞 37669 [796]→[2,4]（特140，施放零额外消耗面）：
//   对手 hp=40000 → 每跳吸 10000（当前体力 1/4，整除精确）；
//   (a) 窗长 2 + 无门：R1 首跳（我施放，自身满血回血钳 0）→ R2 双 PASS 仍结算
//       （无用技门判别）→ R3 窗过期不变；
//   (b) 回血：自身压 3000 → R1 吸 10000 回到 13000。
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

constexpr int kSuiLie = 37669;   // 碎裂晶胞：特140 纯链 [796] → [2,4]
constexpr int kPaDa   = 10004;   // 拍打：filler

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int hp = 40000) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

void both_pass(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2490.a 窗长 2 + 无门 + 当前体力口径（碎裂晶胞 [2,4]）══
    {
        ScenarioBattle b(make_party(self_pet({kSuiLie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // 碎裂晶胞是攻击技：首跳结算在 SKILL_EFFECT 时点（攻击伤害**落地前**）
            // → 吸取按施放时当前体力 40000/4=10000，红伤随后落地（文件头记档）。
            const int final1 = ctx.ws.raw_attack_damage[0];
            CHECK_EQ(ctx.getPet(1).hp, 40000 - 10000 - final1,
                     "T2490.a R1 796：首跳吸取 10000 + 红伤（施放时点口径）");
            CHECK_EQ(ctx.getPet(0).hp, 99999, "T2490.a 796：满血回血钳 0");
            const int hp_base = ctx.getPet(1).hp;   // R1 末（30000-final1）
            both_pass(b);   // R2 PASS 照常结算（无用技门判别）
            const int hp2 = ctx.getPet(1).hp;
            CHECK_EQ(hp_base - hp2, hp_base / 4,
                     "T2490.a R2 796：PASS 回合照吸（无用技门，整除）");
            both_pass(b);   // R3 窗过期 → 不再吸
            CHECK_EQ(ctx.getPet(1).hp, hp2, "T2490.a 796：窗过期不再吸（窗长=2）");
        }
        std::cout << "  [T2490.a] 796 窗长/无门/当前体力口径 通过\n";
    }

    // ══ T2490.b 回血（自身压 3000 → R1 吸 10000 回 13000）══
    {
        ScenarioBattle b(make_party(self_pet({kSuiLie, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 3000;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int finalb = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, 40000 - 10000 - finalb, "T2490.b 796：对手失 10000+红伤");
        CHECK_EQ(ctx.getPet(0).hp, 3000 + 10000, "T2490.b 796：按实际落地回血 3000→13000");
        std::cout << "  [T2490.b] 796 回血 通过\n";
    }

    std::cout << "scenario_2490 PASS (J158 吸取窗族三)" << std::endl;
    return 0;
}
