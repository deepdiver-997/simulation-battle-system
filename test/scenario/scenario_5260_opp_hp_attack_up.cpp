// 场景 5260 —— J201 体力门攻击递增族（487，moves_lib/j_opp_hp_attack_up.cpp）
//（2026-10-22，号段 5260-5269）
//
// 断言口径：
//   - 487（渐落之芳 36882 纯链 [487]|[350,2]，特 140，己方 atk=1）：
//     · (a) 对手 hp=99999 > 350 → 每次使用攻槽 +2：R1 后 +2、R2 后再 +2 累计 +4。
//     · (b) 对手 hp=350 **恰等于**阈值 → 严格 > 不成立 → 攻槽保持 0。
//     · (c) 对手 hp=100 < 阈值 → 不触发（绝对值断言）。
//   - 等级断言读槽位本体 ability_levels[0][0]（精确槽，不用"等级和"）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40
constexpr int kJianLuo = 36882;  // [487]|[350,2]（特 140）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kJianLuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void use_carrier(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5260 487 对手体力 > 阈值 → 每次使用自身攻击 +{1} ══
    {
        {   // (a) 高体力门 → 逐次累加 +2/+4
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[0][0], 2,
                     "T5260.a 487：首次使用攻槽 +2（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[0][0], 4,
                     "T5260.a 487：第二次使用累计 +4（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
        }
        {   // (b) 恰等于阈值 → 严格 > 不成立
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = 350;   // 只压当前体力
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[0][0], 0,
                     "T5260.b 487：hp==350 严格门不触发（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
        }
        {   // (c) 低于阈值 → 不触发
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = 100;
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[0][0], 0,
                     "T5260.c 487：hp=100 不触发（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
        }
        std::cout << "  [T5260] 487 体力门攻击递增 + 阈值边界 + 低体对照 通过\n";
    }

    std::cout << "scenario_5260 PASS (J201 体力门攻击递增族)" << std::endl;
    return 0;
}
