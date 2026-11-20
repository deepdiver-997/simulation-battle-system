// 场景 1290 —— J44 双方同态族之 547（moves_lib/j44_both_sides.cpp）
//（2026-10-02，worktree ../agents/j44；号段 1290-1299 → T1290 单块）
//
// ⚠️ 642 语义未定记档（见实现文件头），本场景只覆盖 547。
//
// 断言口径：
//   - 547 用双侧异常状态精确断言（火焰领地 [2,5]：己方+对手同烧 5 回合；无免疫
//     干扰时两侧都落地）；
//   - 载体：火焰领地 23038 [547]|[2,5] 单效果载体（属性技无红伤）。
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

constexpr int kHuoYan = 23038;  // 火焰领地[547]|[2,5]：属（双烧 5 回合）
constexpr int kPaiDa = 10004;   // 拍打：物40（对手探针）
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1290 547 敌我双方同烧 5 回合（火焰领地 [547]|[2,5]）══
    {
        ScenarioBattle b(make_party(self_pet({kHuoYan, kHuoYan, kHuoYan, kHuoYan, kHuoYan})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T1290 547：己方烧伤落地");
        CHECK(ctx.has_active_abnormal_status(1, kBurn), "T1290 547：对手烧伤落地");
        std::cout << "  [T1290] 547 双方同态 通过\n";
    }

    std::cout << "scenario_1290 PASS (J44 双方同态族 547)" << std::endl;
    return 0;
}
