#include <utils/sbs_rand.h>
// 场景 4530 —— K297 强化态回血族（249，soul_lib/k297_raised_heal_quarter.cpp）
//（2026-10-21，worktree ../agents/k297；号段 4530-4539 → T4530-T4532 三块）
//
// 断言口径（max=800000 → 回血恰 200000；对手 NONE 无扰动；开门直写中性速度槽——
// K283 探针口径：精准/攻击槽会污染伤害面，本单断言面在自身 hp 不受影响）：
//   - T4530 门开：直写速度槽 → 拍打后自 hp = 100000+200000 = 300000。
//   - T4531 门闭：不写槽 → 拍打后自 hp 不变。
//   - T4532 属性技也触发（"所有技能"）：直写槽 → 精准后自 hp = 300000。
//   - T4533 boss 无效：start 前置旗 → 门开也不回。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kZhunQue = 20138;   // 精准：属性技（验证"所有技能"语义）
constexpr int kMaxHp = 800000;

SoulMark make_mark() {
    // DB args 原序 [1,4,1,1]：db[1]=4 回血分母（余废位）。
    SoulMark sm(249, "K297 魂印", "test", EffectArgs(std::vector<int>{1, 4, 1, 1}));
    sm.monster_id = 3151;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "库拉里尔", {kZhunQue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void skill_round(ScenarioBattle& b, int skill) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4530 门开：攻击技 → 回血恰 200000 ═════════════════════════════════════
    {
        sbs_srand(4530);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().ability_levels[0][4] = 1;
        b.ctx().getPet(0).hp = 100000;
        skill_round(b, 1);   // 拍打
        CHECK_EQ(b.ctx().getPet(0).hp, 300000,
                 "T4530 249：强化态攻击技 → 回血 1/4 = 200000（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4530] 门开攻击技回血 通过\n";
    }

    // ══ T4531 门闭：非强化态不回 ═══════════════════════════════════════════════
    {
        sbs_srand(4531);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().getPet(0).hp = 100000;
        skill_round(b, 1);
        CHECK_EQ(b.ctx().getPet(0).hp, 100000,
                 "T4531 249：非强化态 → 不回血（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4531] 门闭对照 通过\n";
    }

    // ══ T4532 属性技也触发（"所有技能"）════════════════════════════════════════
    {
        sbs_srand(4532);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.start();
        b.ctx().ability_levels[0][4] = 1;
        b.ctx().getPet(0).hp = 100000;
        skill_round(b, 0);   // 精准
        CHECK_EQ(b.ctx().getPet(0).hp, 300000,
                 "T4532 249：强化态属性技 → 也回血 200000（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4532] 属性技触发 通过\n";
    }

    // ══ T4533 boss 无效：门开也不回（旗 start 前置位，K294 勘误口径）═══════════
    {
        sbs_srand(4533);
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().ability_levels[0][4] = 1;
        b.ctx().getPet(0).hp = 100000;
        skill_round(b, 1);
        CHECK_EQ(b.ctx().getPet(0).hp, 100000,
                 "T4533 249：boss 无效 → 不回血（实测 hp=" +
                     std::to_string(b.ctx().getPet(0).hp) + "）");
        std::cout << "  [T4533] boss 无效门 通过\n";
    }

    std::cout << "scenario_4530 PASS (K297 强化态回血族)" << std::endl;
    return 0;
}
