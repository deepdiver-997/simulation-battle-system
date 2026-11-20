// 场景 5020 —— J187 主动切换护盾族（1937，moves_lib/j_switch_shield.cpp）
//（2026-10-22，worktree ../agents/j187；号段 5020-5029）
//
// 断言口径：
//   - 1937（织梦 28881 链 [1911,1937]|[240,240]）：R1 施放载体（注册 EVENT_SWAP watcher）
//     → R2 主动中切（slot0→slot1）→ 下只出战获得护盾 240（shield_bank_ 精确 240）。
//     1911 是链上兄弟（切换给护罩 hood_bank_ 240，若已实现）——与护盾槽独立，不干扰。
//   - 对照：R1 不施放 → R2 同样切换 → shield_bank_ 为 0（门禁用绝对值，不写"与对照相等"）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;  // 拍打：物 40（填充刀）
constexpr int kZhiMeng = 28881; // [1911,1937]|[240,240]（切换护罩 240 / 切换护盾 240）
constexpr int kMaxHp = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_active() {
    ElfPet p = make_pet(1, "己方A", {kZhiMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_bench() {
    ElfPet p = make_pet(1, "己方B", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/200, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

std::array<ElfPet, 6> party0() {
    ElfPet a = mk_active();
    ElfPet b = mk_bench();
    return {a, b, b, b, b, b};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5020 1937 主动切换 → 下只出战获护盾 {0} ══
    {
        {   // (a) R1 施放载体 → R2 主动中切 → 下只出战护盾 240
            ScenarioBattle b(party0(), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[0], 1, "T5020.a 前置：slot1 已登场");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 240,
                     "T5020.a 1937：主动切换 → 下只出战获护盾 240（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        {   // (b) 对照：R1 不施放 → R2 同样切换 → 无护盾
            ScenarioBattle b(party0(), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[0], 1, "T5020.b 前置：slot1 已登场");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T5020.b 1937 对照：未施放 → 切换不给护盾（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        std::cout << "  [T5020] 1937 主动切换护盾 + 对照 通过\n";
    }

    std::cout << "scenario_5020 PASS (J187 主动切换护盾族)" << std::endl;
    return 0;
}
