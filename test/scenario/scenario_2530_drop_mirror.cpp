#include <utils/sbs_rand.h>
// 场景 2530 —— J162 降槽反馈族（1 宏，moves_lib/j_drop_mirror.cpp）
//（2026-10-04，worktree ../agents/j162；号段 2530-2539 → T2530 一块）
//
// 断言口径（永黯月辉 28073 [4] cat4 pri0 纯链——属性技纯武装）：
//   a) 预置自身攻 -2 → R1 载体武装 → R2 tick 镜像：对手 slot0=-2；
//      R3 再镜像叠层：对手 slot0=-4（"每回合反馈"字面）。
//   b) 对照：自身无降槽 → 对手槽位恒 0。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40 pri0
constexpr int kYongAn = 28073;  // 永黯月辉 [740]|[4]：属性技（740 载体，pri0）
constexpr int kSelfHp = 5000;
constexpr int kFoeHp  = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
}

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id == 1 ? kSelfHp : kFoeHp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2530.a 降槽镜像（永黯月辉）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kYongAn, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 0, 0, -2);   // 自身攻 -2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体武装
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -2,
                 "T2530.a 740：R2 镜像 -2（实测 " +
                     std::to_string(ctx.ability_levels[1][0]) + "）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -4,
                 "T2530.a 740：R3 再镜像叠层 -4（实测 " +
                     std::to_string(ctx.ability_levels[1][0]) + "）");
        std::cout << "  [T2530.a] 740 降槽镜像 通过\n";
    }

    // ══ T2530.b 对照：无降槽不镜像 ══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kYongAn, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 2; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
        }
        CHECK(ctx.ability_levels[1][0] == 0 && ctx.ability_levels[1][4] == 0,
              "T2530.b 740 对照：无降槽无镜像（实测 " +
                  std::to_string(ctx.ability_levels[1][0]) + "/" +
                  std::to_string(ctx.ability_levels[1][4]) + "）");
        std::cout << "  [T2530.b] 740 无降槽对照 通过\n";
    }

    std::cout << "scenario_2530 PASS (J162 降槽反馈族 740)" << std::endl;
    return 0;
}
