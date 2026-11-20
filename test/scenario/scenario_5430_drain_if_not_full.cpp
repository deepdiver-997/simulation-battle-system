// 场景 5430 —— J216 未满体力吸取族（1371，moves_lib/j_drain_if_not_full.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5430-5439）
//
// 断言口径（绝对体力，伤害浮动不参与）：
//   - 1371（秘影毒瘴 35688 链 [743,1371]|[1,2,3]）：{0}=3 → 吸对手 max_hp/3。
//   - a) 自身 2000 < max 5000（未满）→ 吸取对手 3000/3=1000 → 自身 3000。
//   - b) 自身满血 5000 → 门闭不吸 → 自身保持 5000。
//   - 兄弟 743（反转对手提升+先制）在对手无提升时为空操作，不碰体力。
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
constexpr int kMiYing  = 35688;  // [743,1371]|[1,2,3]（未满吸取 max/3）
constexpr int kSelfMax = 5000;
constexpr int kFoeMax  = 3000;
constexpr int kDrain   = kFoeMax / 3;  // 1000

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kMiYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    // ══ T5430 1371 未满体力 → 吸 max/3；满血门闭 ══
    {
        {   // (a) 自身未满 → 吸 1000
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 2000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 2000 + kDrain,
                     "T5430.a 1371：未满→吸 max/3=1000（self hp 实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 3000）");
        }
        {   // (b) 自身满血 → 不吸
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kSelfMax,
                     "T5430.b 1371 对照：满血门闭（self hp 实测 " +
                         std::to_string(ctx.getPet(0).hp) + "，应 5000）");
        }
        std::cout << "  [T5430] 1371 未满吸取 + 满血门闭 通过\n";
    }

    std::cout << "scenario_5430 PASS (J216 未满体力吸取族)" << std::endl;
    return 0;
}
