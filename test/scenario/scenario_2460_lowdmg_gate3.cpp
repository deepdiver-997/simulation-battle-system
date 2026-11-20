#include <utils/sbs_rand.h>
// 场景 2460 —— J151 低伤门族三（3 宏，moves_lib/j_lowdmg_gate3.cpp）
//（2026-10-04，worktree ../agents/j151；号段 2460-2469 → T2460-T2462 三块）
//
// 断言口径：
//   - 949（炫宇清波 [280] 特130 prio0 纯链）：
//     a) self=2000、atk=1（红伤 1-2 < 280）→ 回满 5000；
//     b) self=2000、atk=999 → 不回；
//     c) seq 门：R1 低伤回满后，R2 拍打（红伤 1-2<280）不再回（hp 定在 2500 不动）。
//   - 1041（残影绝尘断 [280,1] 特160 prio0；链 1052 无提升不发声）：
//     a) R1 atk=1（<280）→ 封技：R2 对手拍打落空（self hp 不动）；R3 封已耗 → 落伤；
//     b) 对照：R1 atk=999 → 不封 → R2 拍打落伤。
//   - 1050（淡雅花香 [2,100,1] 特140 prio0 纯链 100% 确定档；R2/R3 用拍打计数、
//     避开每用重武装的自缠）：
//     R1 载体（atk=1 未击败）→ 武装 2 次；R2 拍打（计 1）→ 未中毒；R3 拍打（计 2）
//     → 中毒（码 1）。
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

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kXuanYu  = 30062;  // 炫宇清波 [949]|[280]：特130
constexpr int kCanYing = 30234;  // 残影绝尘断 [1052,1041]|[280,1]：特160
constexpr int kDanYa   = 35904;  // 淡雅花香 [1050]|[2,100,1]：特140
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
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

    // ══ T2460 949 低伤回满（炫宇清波）══
    {
        // a) 低伤 → 回满
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXuanYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000,
                     "T2460.a 949：低伤回满（实测 " + std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 高伤 → 不回
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXuanYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 999)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 2000,
                     "T2460.b 949：高伤不回（实测 " + std::to_string(ctx.getPet(0).hp) + "）");
        }
        // c) seq 门：R2 拍打低伤不再回
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXuanYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体（回满）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            ctx.getPet(0).hp = 2500;   // R2 前压低血线
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（低伤）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 2500,
                     "T2460.c 949：seq 门——后续低伤不触发（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T2460] 949 低伤回满 通过\n";
    }

    // ══ T2461 1041 低伤封技（残影绝尘断）══
    {
        // a) 低伤 → 封 1 次：R2 拍打落空、R3 落伤
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kCanYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体（低伤封技）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 己方拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手拍打（被封）
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp0,
                     "T2461.a 1041：封技当回合对手攻击无效（实测掉血 " +
                         std::to_string(hp0 - ctx.getPet(0).hp) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp0,
                  "T2461.a 1041：封耗后攻击落伤（实测 " + std::to_string(ctx.getPet(0).hp) +
                      " < " + std::to_string(hp0) + "）");
        }
        // b) 对照：高伤不封 → R2 拍打落伤
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kCanYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 999)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp < hp0,
                  "T2461.b 1041 对照：高伤不封（实测 " + std::to_string(ctx.getPet(0).hp) +
                      " < " + std::to_string(hp0) + "）");
        }
        std::cout << "  [T2461] 1041 低伤封技 通过\n";
    }

    // ══ T2462 1050 未击败计数异常（淡雅花香，100% 档）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kDanYa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体（未击败 → 武装 2）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, /*中毒=*/1),
              "T2462 前置：R1 未中毒");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（计 1）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(!ctx.has_active_abnormal_status(1, /*中毒=*/1),
              "T2462 1050：计 1 未到 2，不施加");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 拍打（计 2 → 触发）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, /*中毒=*/1),
              "T2462 1050：计满 2 → 100% 中毒");
        std::cout << "  [T2462] 1050 未击败计数异常 通过\n";
    }

    std::cout << "scenario_2460 PASS (J151 低伤门族三 949/1041/1050)" << std::endl;
    return 0;
}
