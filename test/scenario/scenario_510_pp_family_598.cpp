#include <utils/sbs_rand.h>
// 场景 510：PP 族批量效果 598"{0}%恢复自己所有技能PP值{1}点"
//（moves_lib/e_pp_family.cpp，E6，2026-09-29）
//
// 载体：明语释心 28449（链=[598]、切片 [100,5] → 100% 全技能 +5 PP；属性技、maxPP=10）。
//
// 断言组：
//   TA 恢复+钳满：不满槽精确 +5（0→5 / 3→8 / 1→6），近满槽钳到 maxPP（33→35）；
//      载体自身槽用后 -1 再 +5 → 钳回 10。
//   TB 对手不受影响：598 只恢复自己（对手拍打仅自身消耗 35→34）。
//   TC 确定性：同种子两场终局 PP 向量逐槽一致（受管 RNG）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kMingYu = 28449;   // 明语释心：属性技、maxPP=10、effect [598]=[100,5]
constexpr int kPaDa = 10004;     // 拍打：maxPP=35
constexpr int kRestore = 5;      // 598 切片 {1}=5
constexpr int kPaDaMax = 35;

ElfPet caster() {
    ElfPet p = scenario::make_pet(5000, "谱尼", {kMingYu, kPaDa, kPaDa, kPaDa, kPaDa},
                                  /*speed=*/200, /*atk=*/150, /*hp=*/9999);
    p.skills[0].must_hit = 1;   // 消除 miss 分支（accuracy 95），保确定性
    return p;
}

ElfPet foe() {
    return scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                              /*speed=*/100, /*atk=*/10, /*hp=*/9999);
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 恢复 +5、近满钳 maxPP、自身槽用后再补 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(caster()), scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 铺四档不满槽：0（全空）/ 3（+5 无钳）/ 1（+5 无钳）/ 33（近满钳 35）
        ctx.seerRobot[0].elfPets[0].skills[1].pp = 0;
        ctx.seerRobot[0].elfPets[0].skills[2].pp = 3;
        ctx.seerRobot[0].elfPets[0].skills[3].pp = 1;
        ctx.seerRobot[0].elfPets[0].skills[4].pp = 33;
        drive_round(b, 0, 0);   // 明语释心（100% 触发）
        CHECK_EQ(ctx.getPet(0).skills[1].pp, kRestore, "TA：0→5（+5 无钳）");
        CHECK_EQ(ctx.getPet(0).skills[2].pp, 3 + kRestore, "TA：3→8（+5 无钳）");
        CHECK_EQ(ctx.getPet(0).skills[3].pp, 1 + kRestore, "TA：1→6（+5 无钳）");
        CHECK_EQ(ctx.getPet(0).skills[4].pp, kPaDaMax, "TA：33→35（钳 maxPP）");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 9,
                 "TA：载体自身时序 = 10 -(SKILL_EFFECT 时 +5 钳 10)- 1(消耗在后) → 9（实际 "
                 << ctx.getPet(0).skills[0].pp << "）");
        std::cout << "  [TA] 598 恢复+钳满 通过\n";
    }

    //========== TB 只恢复自己：对手 PP 仅自身消耗 ==========
    {
        sbs_srand(20260929);
        ScenarioBattle b(scenario::make_party(caster()), scenario::make_party(foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[1].elfPets[0].skills[0].pp = 20;   // 对手拍打压到 20（非满，便于辨认）
        drive_round(b, 0, 0);
        CHECK_EQ(ctx.getPet(1).skills[0].pp, 19, "TB 核心：对手 20→19（仅自身消耗，不被恢复）");
        CHECK_EQ(ctx.getPet(1).skills[1].pp, kPaDaMax, "TB 对手其余槽不动");
        std::cout << "  [TB] 598 只恢复自己 通过\n";
    }

    //========== TC 确定性：同种子两场终局一致 ==========
    {
        int pa[5] = {0, 0, 0, 0, 0};
        int pb[5] = {-1, -1, -1, -1, -1};
        for (int run = 0; run < 2; ++run) {
            sbs_srand(987654321);
            ScenarioBattle b(scenario::make_party(caster()), scenario::make_party(foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int i = 1; i < 5; ++i) {
                ctx.seerRobot[0].elfPets[0].skills[i].pp = 7;   // 统一 7 → 12
            }
            drive_round(b, 0, 0);
            for (int i = 0; i < 5; ++i) {
                (run == 0 ? pa : pb)[i] = ctx.getPet(0).skills[i].pp;
            }
        }
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(pa[i], pb[i], "TC 核心：同种子两场槽 " << i << " 终局一致（"
                                     << pa[i] << " vs " << pb[i] << "）");
        }
        CHECK_EQ(pa[1], 7 + kRestore, "TC 附带：7→12（+5 无钳）");
        std::cout << "  [TC] 598 受管 RNG 确定性 通过\n";
    }

    std::cout << "scenario_510 all passed" << std::endl;
    return 0;
}
