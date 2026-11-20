// 场景 1310 —— J46 低伤罚窗族之 742（moves_lib/j46_low_dmg_punish.cpp）
//（2026-10-02，worktree ../agents/j46；号段 1310-1319 → T1310 单块）
//
// 断言口径：
//   - a) 对手 atk=1（每回合伤害 1~2 < 250）→ R1 末即吃害怕（{2}=6）；
//   - b) 对手 atk=3000（每回合 > 250）→ 三回合均不罚；
//   - 载体：末世天崩诀 17360 [742]|[3,250,6] 单效果+零参无害链（180=攻击+）。
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

constexpr int kMoShi = 17360;  // 末世天崩诀[742,180]|[3,250,6]：物160（低伤罚窗）
constexpr int kPaiDa = 10004;  // 拍打：物40
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
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

    // ══ T1310 742 对手低伤→害怕（末世天崩诀 [742]|[3,250,6]）══
    {
        {
            // a) 低伤：atk=1 → R1 末罚
            ScenarioBattle b(
                make_party(self_pet({kMoShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手拍打
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T1310.a 742：对手回合伤害 <250 → 害怕落地");
        }
        {
            // b) 高伤：atk=3000 → 三回合均不罚
            ScenarioBattle b(
                make_party(self_pet({kMoShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            for (int r = 0; r < 2; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
            }
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T1310.b 742 门：对手高伤不罚");
        }
        std::cout << "  [T1310] 742 低伤罚窗 通过\n";
    }

    std::cout << "scenario_1310 PASS (J46 低伤罚窗族 742)" << std::endl;
    return 0;
}
