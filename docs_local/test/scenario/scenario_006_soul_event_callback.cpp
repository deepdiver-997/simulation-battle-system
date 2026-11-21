// 场景 F：启灵元神 1581 魂印 [神印] —— 对方主动切换登场 → 立即挂层 + 立即按层数真伤（事件回调合并执行）。
//
// 机制（数据库 effect_icon id=1246, effect_id=1581, pet_id=3986 启灵元神）：
//   "自身在出战背包时，对方所有精灵主动切换登场会附加 1 层神印（最多 3 层）"。
//     1 层 = 对方最大体力 1/8 真伤；2 层 = 1/6；3 层 = 1/4
//   注：神印层数**不**在每次切完后清零——按用户"神印无法减少"持续累计（3 层封顶）。
//
// 架构（重构后）：
//   - 启灵元神方在 OPERATION_ENTER_EXIT_STAGE 注册 EVENT_SWAP watcher（永久）
//   - 回调内：event.actor == 1-self（对方在切）→ 给 actor 自身 soulmark_storage[1581].stacks += 1
//     （截断 ≤3）→ 立即按层数对 actor 扣真伤
//   - emit 时点：handle_OperationChooseSkillMedicament 收到 CHOOSE_PET 时
//     （perform_switch 也同步完成 → drain 时 getPet(actor) 看到新精灵）
//   - 双方都带启灵元神：双方都注册 watcher，event.actor 互相对应 → 各管各的对手中切。
//
// 验证（双方纯切宠不打架，直观预测每个 pet 切上来到结算后的 hp）：
//   对照组：无魂印 → 双方任何切换都不扣血，所有 pet 永远 1000hp。
//   主场景：0 号带 1581，1 号不带。双方都每回合切宠（用同一只精灵反复切上 → 反复吃神印 → 层数累计）。
//     0 号的精灵：0 号是 watcher 持有者，event.actor=0 时不触发；0 号切换永远 0 扣血。
//     1 号的精灵：1 号是被挂方，每切一次登场 stacks+1、按层数真伤；同一只精灵反复切上来层数会累计。
//       公式：本次切换后 pet.hp = 切换前 pet.hp - max(1, 1000 / 分母(stacks_after))
//         stacks 0→1: 扣 1000/8 = 125
//         stacks 1→2: 扣 1000/6 = 166
//         stacks 2→3: 扣 1000/4 = 250
//         stacks 3→3（封顶）: 仍扣 1000/4 = 250
#include <cassert>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

// 技能 id（撞击：无效果，priority 0；本场景不出招，技能不影响验证）
constexpr int kSkillZhuangJi = 10001;

// 双方 6 槽 pet 都是 make_pet 的同一只复制（HP=1000，元素 0），但 0 号槽 0 可挂 1581。
// 测试只切宠不动手，每回合双方都 CHOOSE_PET。
struct SwitchStep {
    int target_slot_0;   // 本回合 0 号切到的目标槽
    int target_slot_1;   // 本回合 1 号切到的目标槽
};

// 控制组：双方都不带 1581 → 所有切换后 0 号 / 1 号 pet 永远 1000hp。
void test_control_no_1581() {
    auto pet0 = scenario::make_pet(0, "对照0号(无魂印)",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/1000);
    auto pet1 = scenario::make_pet(1, "对照1号(无魂印)",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/1000);
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();

    // 双方每回合切到不同槽：0 号 1→2→1→2→1，1 号 1→2→3→1→2（来回调换）
    const std::vector<SwitchStep> steps = {
        {1, 1},  // R1: 0号切 槽1, 1号切 槽1
        {2, 2},  // R2: 0号切 槽2, 1号切 槽2
        {1, 3},  // R3: 0号切 槽1, 1号切 槽3
        {2, 1},  // R4: 0号切 槽2, 1号切 槽1
        {1, 2},  // R5: 0号切 槽1, 1号切 槽2
    };
    for (size_t r = 0; r < steps.size(); ++r) {
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, steps[r].target_slot_0);
        battle.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, steps[r].target_slot_1);
        battle.drive();
        // 对照组：0 号、1 号 所有 pet hp 都应仍 = 1000
        for (int slot = 0; slot < 6; ++slot) {
            const int p0 = battle.ctx().seerRobot[0].elfPets[slot].hp;
            const int p1 = battle.ctx().seerRobot[1].elfPets[slot].hp;
            assert(p0 == 1000);
            assert(p1 == 1000);
        }
    }
    std::cout << "  [control] R1~R5 双方各槽均 HP=1000（无神印无扣血）" << std::endl;
}

// 主场景：0 号带 1581，1 号不带。0 号切换不扣血（0 号是 watcher 持有者），1 号切换按层数扣血。
// 1 号反复切同一只精灵展示层数累计：0→1(扣125)→2(扣166)→3(扣250)→3(扣250)→...
void test_main_1581_on_0() {
    auto pet0 = scenario::make_pet(0, "启灵元神0号(1581持有者)",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/1000,
                                   /*soul_mark=*/SoulMark(1581, "启灵元神神印", ""));
    auto pet1 = scenario::make_pet(1, "1号(无魂印，神印被挂方)",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/1000);
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();

    // 序列：
    //   R1: 0号切槽 0(原地)  1号切槽 1（首次切到 1号 petA）→ 1号petA: 1000-125=875
    //   R2: 0号切槽 0(原地)  1号切槽 2（首次切到 1号 petB）→ 1号petB: 1000-125=875
    //   R3: 0号切槽 0(原地)  1号切槽 1（再次切 1号 petA：1层→2层）→ 1号petA: 875-166=709
    //   R4: 0号切槽 0(原地)  1号切槽 2（再次切 1号 petB：1层→2层）→ 1号petB: 875-166=709
    //   R5: 0号切槽 0(原地)  1号切槽 1（再次切 1号 petA：2层→3层）→ 1号petA: 709-250=459
    //   R6: 0号切槽 0(原地)  1号切槽 2（再次切 1号 petB：2层→3层）→ 1号petB: 709-250=459
    //   R7: 0号切槽 0(原地)  1号切槽 1（再次切 1号 petA：3层→3层封顶）→ 1号petA: 459-250=209
    //
    // 0 号的所有 pet: 0 号是 watcher 持有者（event.actor=0 不触发），永远 HP=1000。
    // 1 号的 pet 0: 从未切上，永远 HP=1000。
    // 1 号的 pet 3,4,5: 从未切上，永远 HP=1000。
    const std::vector<SwitchStep> steps = {
        {0, 1},  // R1
        {0, 2},  // R2
        {0, 1},  // R3
        {0, 2},  // R4
        {0, 1},  // R5
        {0, 2},  // R6
        {0, 1},  // R7
    };
    for (size_t r = 0; r < steps.size(); ++r) {
        battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, steps[r].target_slot_0);
        battle.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, steps[r].target_slot_1);
        battle.drive();
    }
    // 验证 R6 末状态
    std::cout << "  [main r6] 0号(持有者) 槽0~5 hp: ";
    for (int slot = 0; slot < 6; ++slot) {
        std::cout << battle.ctx().seerRobot[0].elfPets[slot].hp << " ";
    }
    std::cout << "(均应为 1000 — 0号是watcher持有者，event.actor=0 不触发)\n";

    std::cout << "  [main r6] 1号(被挂方) 槽0~5 hp: ";
    for (int slot = 0; slot < 6; ++slot) {
        std::cout << battle.ctx().seerRobot[1].elfPets[slot].hp << " ";
    }
    std::cout << "\n";
    // 0 号所有 pet 永远 HP=1000
    for (int slot = 0; slot < 6; ++slot) {
        assert(battle.ctx().seerRobot[0].elfPets[slot].hp == 1000);
    }
    // 1 号各 pet 状态
    assert(battle.ctx().seerRobot[1].elfPets[0].hp == 1000);  // pet0: 从未切上
    assert(battle.ctx().seerRobot[1].elfPets[1].hp == 209);   // pet1: R1(125)+R3(166)+R5(250)+R7(250) = 791 扣，1000-791=209
    assert(battle.ctx().seerRobot[1].elfPets[2].hp == 459);   // pet2: R2(125)+R4(166)+R6(250) = 541 扣，1000-541=459
    assert(battle.ctx().seerRobot[1].elfPets[3].hp == 1000);  // pet3: 从未切上
    assert(battle.ctx().seerRobot[1].elfPets[4].hp == 1000);  // pet4: 从未切上
    assert(battle.ctx().seerRobot[1].elfPets[5].hp == 1000);  // pet5: 从未切上
    // 注：stacks 累计的行为已经通过 HP 隐式验证——petA 第 4 次切上若 stacks 清零，
    // 只扣 125，则 HP=750；实际 HP=209 说明 4 次切上都按累计层数扣了 125+166+250+250=791。
    // ShenyinStorage 类型在插件内（匿名 namespace），不在头文件暴露，故不直接 any_cast 校验。
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    std::cout << "[scenario_f] control: 双方无 1581，6 回合切来切去" << std::endl;
    test_control_no_1581();

    std::cout << "[scenario_f] main: 0号挂 1581，1号不带，6 回合切来切去" << std::endl;
    test_main_1581_on_0();

    std::cout << "scenario_f PASS (启灵元神 1581 神印：单回调挂层+真伤，stacks 不清零)"
              << std::endl;
    return 0;
}
