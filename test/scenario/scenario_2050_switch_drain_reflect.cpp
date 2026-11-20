// 场景 2050 —— J111 切换追击/伤害反馈族（2 宏，moves_lib/j_switch_drain_reflect.cpp）
//（2026-10-03，worktree ../agents/j111；号段 2050-2059 → T2050-T2051 两块）
//
// 断言口径：
//   - 1779（牵风引霞 [1779,1702✓哑]|[1,3]——1702 为 J98 未合并效果，本基线不注册）：
//     R1 载体（计数 1、分母 3）；R2 对手主动中切（新宠 99999 预置）→ 吸 1/3=33333
//     精确（drain 无浮动）；R3 对手再切 → 计数耗尽不吸（新宠满血不变）；
//   - 855（炽霆灼击 [50] 物120 纯链）：R1 挂反馈；R2 对手攻击命中我方（~480±）
//     → 反馈 50% ≈240 给对手（对手掉血 = 我方拍打~2 + 反馈，带 [230,255]）；
//     对照 = 我方未受击 → 无反馈。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kQianFeng = 36703; // 牵风引霞 [1779,1702]|[1,3,300]（切换追击载体）
constexpr int kChiTing = 36478;  // 炽霆灼击 [855]|[50]：物120（伤害反馈载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, 100, atk, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2050 1779 对手切换 → 吸 1/3（牵风引霞 [1,3]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kQianFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int slot = 0; slot < 6; ++slot) {
            ctx.seerRobot[1].elfPets[slot].hp = 99999;   // 对手全队整数血好算
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        // R2：对手主动中切（slot0 → slot1，新宠 99999 登场）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        // ⚠️ ENTER 事件派发晚一个 drive 边界（J62 教训同款）——空转一拍再断言
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 99999 - 33333,
                 "T2050.a 1779：对手切换 → 吸下只 1/3=33333（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        // R4：对手再切 → 计数耗尽不吸（再空转一拍供派发）
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
        b.drive();
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 99999,
                 "T2050.b 1779：计数耗尽 → 新登场宠不被吸（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T2050] 1779 切换追击 通过\n";
    }

    // ══ T2051 855 下次受击反馈 50%（炽霆灼击 [50]）══
    {
        // a) R1 挂反馈；R2 对手攻击命中 → 反馈 50%
        {
            ScenarioBattle b(
                make_party(mk_self({kChiTing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1500)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂反馈
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_hp1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 我方拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手攻击
            b.drive();
            const int foe_loss = foe_hp1 - ctx.getPet(1).hp;
            CHECK(foe_loss >= 230 && foe_loss <= 255,
                  "T2051.a 855：受击反馈 50%（对手掉 " + std::to_string(foe_loss) +
                      " = 拍打~2 + 反馈）");
        }
        // b) 对照：我方未受击 → 无反馈
        {
            ScenarioBattle b(
                make_party(mk_self({kChiTing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1500)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂反馈
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_hp1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = foe_hp1 - ctx.getPet(1).hp;
            CHECK(foe_loss <= 5,
                  "T2051.b 855 对照：未受击无反馈（实测掉 " +
                      std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T2051] 855 伤害反馈 通过\n";
    }

    std::cout << "scenario_2050 PASS (J111 切换追击/伤害反馈族 1779/855)" << std::endl;
    return 0;
}
