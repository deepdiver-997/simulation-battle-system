// 场景 4300 —— K274 首战挡两次伤害族（620，soul_lib/k274_first_battle_block.cpp）
//（2026-10-21，worktree ../agents/k274；号段 4300-4309 → T4300-T4301 两块）
//
// 断言口径（620「首次出战时可以抵挡两次攻击伤害（boss无效）」）：
//   - T4300：印主持 620 登场 → 攻方前两次攻击**被完全挡下**（掉血 0），
//     第 3 次正常掉血（次数用尽）——场景 031 Q3（counts=2）同型范式。
//   - T4301 对照：无魂印的印主 → 第 1 次攻击就正常掉血（门闭对照）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,1}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击载体

// 攻方：speed 300（恒先手），atk 100（拍打有伤害但不致命）。
ElfPet make_atk() {
    ElfPet p = make_pet(0, "攻方", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/100, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 守方（持 620 魂印）。
ElfPet make_def_mark() {
    std::vector<int> args{2, 1};
    ElfPet p = make_pet(1, "守方", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(620, "K274 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 守方（无魂印对照）。
ElfPet make_def_plain() {
    ElfPet p = make_pet(1, "守方", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 跑一回合（双方都出招），返回守方本回合掉血。
int attack_round(ScenarioBattle& b) {
    const int hp_before = b.ctx().getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return hp_before - b.ctx().getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4300：前两次攻击被挡下，第三次正常 ════════════════════════════════
    {
        ScenarioBattle b(make_party(make_atk()), make_party(make_def_mark()));
        b.start();
        const int dmg1 = attack_round(b);
        const int dmg2 = attack_round(b);
        const int dmg3 = attack_round(b);
        CHECK_EQ(dmg1, 0, "T4300 620：第 1 次攻击被完全挡下");
        CHECK_EQ(dmg2, 0, "T4300 620：第 2 次攻击被完全挡下（db[0]=2 次）");
        CHECK(dmg3 > 0, "T4300 620：两次用尽 → 第 3 次攻击正常造成伤害");
        std::cout << "  [T4300] 首战挡两次 通过（dmg=" << dmg1 << "/" << dmg2
                  << "/" << dmg3 << "）\n";
    }

    // ══ T4301 对照：无魂印 → 第 1 次就正常掉血 ══════════════════════════════
    {
        ScenarioBattle b(make_party(make_atk()), make_party(make_def_plain()));
        b.start();
        const int dmg1 = attack_round(b);
        CHECK(dmg1 > 0, "T4301 620：对照组（无魂印）第 1 次攻击正常掉血（门闭）");
        std::cout << "  [T4301] 门闭对照 通过（dmg=" << dmg1 << "）\n";
    }

    // ══ T4302 换宠不上重挂：守方换宠后，票数不重置（仍是 2 次）══════════════
    {
        ScenarioBattle b(make_party(make_atk()), make_party(make_def_mark()));
        b.start();
        const int d1 = attack_round(b);   // 第 1 次 → 挡下
        const int d2 = attack_round(b);   // 第 2 次 → 挡下
        // 守方换宠（CHOOSE_PET → 槽 1）：新宠上场会再次触发登场节点 → 若"首次"
        // 门失效则会**重挂票**（次数变 4）→ 后续攻击应仍被挡（与不重挂同观）。
        // 本例断言换宠后第 3 次攻击**正常掉血**（票已用尽、未被重挂延长）。
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        const int d3 = attack_round(b);
        CHECK_EQ(d1, 0, "T4302 620：换宠前第 1 次被挡下");
        CHECK_EQ(d2, 0, "T4302 620：换宠前第 2 次被挡下");
        CHECK(d3 > 0, "T4302 620：换宠后票不重挂（次数已用尽 → 第 3 次正常掉血）");
        std::cout << "  [T4302] 换宠不重挂 通过（d=" << d1 << "/" << d2 << "/" << d3
                  << "）\n";
    }

    std::cout << "scenario_4300 PASS (K274 首战挡两次伤害族)" << std::endl;
    return 0;
}
