// 场景 5410 —— J214 清盾罩附粉伤族（1828，moves_lib/j_clear_shields_pink.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5410-5419）
//
// 断言口径（精确槽位 + 绝对体力）：
//   - 1828（浅唱诗 28632 纯链 [1828]|[150]，属性技无攻击伤害）：对手护盾 300 + 护罩 200
//     → 两清；附伤 = (300+200)×150% = 750 百分比伤害 → 对手 10000→9250。
//   - 对照：对手无盾无罩 → 两清无事、无附伤（血量不变 10000）。
//   - 粉伤走真实 deal_pink_damage 管线（PERCENT_VALUE 档，吃免粉/抗性/护罩——护罩已被
//     清空，不再吸收），不是直写 hp。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kQianChang = 28632; // [1828]|[150]（纯链；两清 + 消除总和 150% 粉伤）
constexpr int kSelfMax = 5000;
constexpr int kFoeMax  = 10000;
constexpr int kShield  = 300;
constexpr int kHood    = 200;
constexpr int kPink    = (kShield + kHood) * 150 / 100;  // 750

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kQianChang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kSelfMax);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kFoeMax);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5410 1828 两清 + 消除总和 150% 粉伤 ══
    {
        {   // (a) 护盾 300 + 护罩 200 → 两清、附伤 750
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).shield_bank_.add(/*priority=*/0, kShield, /*source_id=*/1111,
                                           /*duration=*/0);
            ctx.getPet(1).hood_bank_.add(/*priority=*/0, kHood, /*source_id=*/2222,
                                         /*duration=*/0);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).shield_bank_.total(), 0,
                     "T5410.a 1828：护盾清空（实测 " +
                         std::to_string(ctx.getPet(1).shield_bank_.total()) + "）");
            CHECK_EQ(ctx.getPet(1).hood_bank_.total(), 0,
                     "T5410.a 1828：护罩清空（实测 " +
                         std::to_string(ctx.getPet(1).hood_bank_.total()) + "）");
            CHECK_EQ(ctx.getPet(1).hp, kFoeMax - kPink,
                     "T5410.a 1828：附伤 500×150%=750（foe hp 实测 " +
                         std::to_string(ctx.getPet(1).hp) + "，应 9250）");
        }
        {   // (b) 对照：无盾无罩 → 不附伤
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, kFoeMax,
                     "T5410.b 1828 对照：无盾罩无附伤（foe hp 实测 " +
                         std::to_string(ctx.getPet(1).hp) + "，应 10000）");
        }
        std::cout << "  [T5410] 1828 两清 + 150% 粉伤 + 无盾对照 通过\n";
    }

    std::cout << "scenario_5410 PASS (J214 清盾罩附粉伤族)" << std::endl;
    return 0;
}
