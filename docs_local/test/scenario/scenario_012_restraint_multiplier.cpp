// 场景 L：克制倍率修复验证——官方 types_relation 原始倍率 {0免疫, 0.5减半, 1普通, 2克制}。
//
// 克制按"技能系别 vs 防御方系别"计算（官方机制）：攻击方精灵元素只影响本系加成(involve)，
// 不影响克制。因此测试须用同系别技能而非"任意技能+改精灵元素"：
//   草系技能 疾风刃(10003) 打水 → 2.0 克制 / 打草 → 0.5 减半
//   电系技能 电流撞击(10106) 打地面 → 0.0 免疫
//
// 元素 id（skill_types）：草=1 水=2 电=5 地面=7 普通=8。
// 验证：
//   ① 矩阵值：草克水=2.0 / 草对草=0.5 / 电对地=0.0
//   ② 战斗：草打水 伤害 ≈ 2x；草打草 伤害 ≈ 0.5x（4 倍差）
//   ③ 免疫：电打地 伤害 = 0
#include <cassert>
#include <iostream>

#include <entities/elemental-attributes.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kElementGrass = 1;    // 草
constexpr int kElementWater = 2;    // 水
constexpr int kElementElectric = 5; // 电
constexpr int kElementGround = 7;   // 地面
constexpr int kSkillZhuangJi = 10001;  // 撞击：元素普通(8)，防御方用（伤害断言只看攻击方）
constexpr int kSkillGrass = 10003;     // 疾风刃：元素草(1)，无 side_effect
constexpr int kSkillElectric = 10106;  // 电流撞击：元素电(5)，无 side_effect

// 构造指定精灵元素 + 指定技能（克制按技能系别算，故攻击技能须与要测的元素一致）。
ElfPet make_pet_with_element(int slot_id, int element, int skill_id) {
    auto pet = scenario::make_pet(slot_id, "我方", {skill_id, skill_id, skill_id,
                                                    skill_id, skill_id},
                                  /*speed=*/200, /*atk=*/150, /*hp=*/800);
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    return pet;
}

ElfPet make_opponent(int element, int hp = 5000) {
    auto pet = scenario::make_pet(1, "对手", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                              kSkillZhuangJi, kSkillZhuangJi},
                                  /*speed=*/100, /*atk=*/100, /*hp=*/hp);
    pet.elementalAttributes[0] = element;
    pet.elementalAttributes[1] = 0;
    return pet;
}

// 打一回合，返回 pet1 掉血。
int damage_dealt(ElfPet pet0, ElfPet pet1) {
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    return 5000 - battle.ctx().getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ① 矩阵值：官方原始倍率直接可用
    {
        const auto& m = ElementalAttributes::elementalAttributesRestraints;
        assert(m[kElementGrass][kElementWater] == 2.0);   // 草克水
        assert(m[kElementGrass][kElementGrass] == 0.5);   // 草对草减半
        assert(m[kElementElectric][kElementGround] == 0.0); // 电对地免疫
        std::cout << "  [matrix] 草克水=2.0 草对草=0.5 电对地=0.0 (官方倍率直存)" << std::endl;
    }

    // ② 战斗伤害：草系技能打水（2x）明显高于草系技能打草（0.5x）
    {
        const int dmg_water = damage_dealt(make_pet_with_element(0, kElementGrass, kSkillGrass),
                                           make_opponent(kElementWater));
        const int dmg_grass = damage_dealt(make_pet_with_element(0, kElementGrass, kSkillGrass),
                                           make_opponent(kElementGrass));
        // 2x vs 0.5x = 4 倍差；随机 ±8% 不影响方向性
        assert(dmg_water > 0 && dmg_grass > 0);
        assert(dmg_water >= dmg_grass * 2);
        std::cout << "  [battle] 草打水=" << dmg_water << " 草打草=" << dmg_grass
                  << " (克制 vs 减半)" << std::endl;
    }

    // ③ 免疫：电系技能打地面 伤害 0
    {
        const int dmg = damage_dealt(make_pet_with_element(0, kElementElectric, kSkillElectric),
                                     make_opponent(kElementGround));
        assert(dmg == 0);
        std::cout << "  [immune] 电打地=" << dmg << " (0.0 免疫)" << std::endl;
    }

    std::cout << "scenario_l PASS (克制倍率：0.5减半/2.0克制/0.0免疫均按官方倍率生效)"
              << std::endl;
    return 0;
}
