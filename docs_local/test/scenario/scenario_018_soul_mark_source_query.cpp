// 场景 R：魂印源空间查询原语 find_pet_with_soulmark（档案 §4 的地基）。
//
// 用途（docs/02-效果系统/魂印机制设计与精灵表现档案.md §4）：
//   效果执行时判定"我（源精灵）此刻在场上还是场下"：
//     find_pet_with_soulmark(owner, id) == on_stage[owner]  → 源在场上
//     不等 / 返回 -1                                          → 源在出战背包 / 已阵亡
//   驱动场景：薇尔诗 2513"场下精灵无法指定场上精灵为效果目标"、瀚宇星皇 903"星皇场下仍提供 buff"。
//
// 本场景只验证**查询原语本身**（#1），不做 2513 场域抑制与常驻注册（#2/#3 待做）。
// 验证点：
//   P1 战斗开始：携带魂印的精灵在槽 0 → 返回 0，且 == on_stage（源在场上）
//   P2 切到槽 2 后：返回 2（查询跟随 on_stage 变化），源仍在场上
//   P3 携带魂印的精灵在场下（切到别的槽）→ 返回值 != on_stage（源场下）
//   P4 携带魂印的精灵阵亡（hp=0）→ 返回 -1（存活判定）
//   P5 同方 6 槽都无该魂印 → 返回 -1
//   P6 非法参数（owner 越界 / id<=0）→ 返回 -1
//
// 注：本场景手工把魂印挂到非 0 槽的精灵上（make_party 是同一只的 6 份拷贝，
//     所以"槽 2 的精灵带魂印"需要显式往某个槽写）。这模拟"魂印持有者在背包位"。
#include <cassert>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;
// 用于查询的魂印 id（不注册实现，只作为槽位标记；本场景不依赖其效果逻辑）
constexpr int kTestSoulMarkId = 903;

// 造一只带指定魂印的精灵
ElfPet make_pet_with_mark(int id, const std::string& name, int hp, int soulmark_id) {
    return scenario::make_pet(id, name,
                              {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                               kSkillZhuangJi, kSkillZhuangJi},
                              /*speed=*/100, /*atk=*/100, /*hp=*/hp,
                              /*soul_mark=*/SoulMark(soulmark_id, "测试魂印", ""));
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 0 号队伍：槽 0 是不带魂印的普通精灵，槽 2 带测试魂印（模拟"魂印持有者在背包位"）。
    // 1 号队伍：全部不带魂印（用作 P5 反例）。
    auto plain = scenario::make_pet(0, "0号普通",
                                    {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                     kSkillZhuangJi, kSkillZhuangJi},
                                    /*speed=*/100, /*atk=*/100, /*hp=*/1000);
    auto marked = make_pet_with_mark(0, "0号带魂印(槽2)", /*hp=*/1000, kTestSoulMarkId);

    std::array<ElfPet, 6> party0 = {plain, plain, marked, plain, plain, plain};
    std::array<ElfPet, 6> party1 = scenario::make_party(plain);

    scenario::ScenarioBattle battle(party0, party1);
    battle.start();

    BattleContext& ctx = battle.ctx();

    // --- P1：战斗开始，on_stage[0]=0（不带魂印）→ 查询应返回 2，且 != on_stage（源场下）
    const int slot_p1 = ctx.find_pet_with_soulmark(0, kTestSoulMarkId);
    std::cout << "  [P1] 开战 on_stage[0]=" << ctx.on_stage[0]
              << " find=" << slot_p1 << " (期望 2，!= on_stage → 源场下)" << std::endl;
    assert(slot_p1 == 2);
    assert(slot_p1 != ctx.on_stage[0]);

    // --- P2：切到槽 2（带魂印的那只上场）→ 查询仍返回 2，且 == on_stage（源场上）
    battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 2);
    battle.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
    battle.drive();

    const int slot_p2 = ctx.find_pet_with_soulmark(0, kTestSoulMarkId);
    std::cout << "  [P2] 切槽2后 on_stage[0]=" << ctx.on_stage[0]
              << " find=" << slot_p2 << " (期望 2 == on_stage → 源场上)" << std::endl;
    assert(ctx.on_stage[0] == 2);
    assert(slot_p2 == 2);
    assert(slot_p2 == ctx.on_stage[0]);

    // --- P3：再切到槽 4（带魂印的那只回到背包）→ 查询仍返回 2，但 != on_stage（源又变场下）
    battle.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 4);
    battle.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 2);
    battle.drive();

    const int slot_p3 = ctx.find_pet_with_soulmark(0, kTestSoulMarkId);
    std::cout << "  [P3] 切槽4后 on_stage[0]=" << ctx.on_stage[0]
              << " find=" << slot_p3 << " (期望 2 != on_stage → 源场下)" << std::endl;
    assert(ctx.on_stage[0] == 4);
    assert(slot_p3 == 2);
    assert(slot_p3 != ctx.on_stage[0]);

    // --- P4：把槽 2 那只打死（hp=0）→ 查询返回 -1（存活判定：阵亡不提供效果）
    ctx.seerRobot[0].elfPets[2].hp = 0;
    const int slot_p4 = ctx.find_pet_with_soulmark(0, kTestSoulMarkId);
    std::cout << "  [P4] 槽2 阵亡后 find=" << slot_p4 << " (期望 -1)" << std::endl;
    assert(slot_p4 == -1);

    // --- P5：1 号方无任何精灵带该魂印 → -1
    const int slot_p5 = ctx.find_pet_with_soulmark(1, kTestSoulMarkId);
    std::cout << "  [P5] 1号方(无该魂印) find=" << slot_p5 << " (期望 -1)" << std::endl;
    assert(slot_p5 == -1);

    // --- P6：非法参数
    assert(ctx.find_pet_with_soulmark(-1, kTestSoulMarkId) == -1);
    assert(ctx.find_pet_with_soulmark(2, kTestSoulMarkId) == -1);
    assert(ctx.find_pet_with_soulmark(0, 0) == -1);
    assert(ctx.find_pet_with_soulmark(0, -5) == -1);
    std::cout << "  [P6] 非法参数(owner=-1/2, id=0/-5) 均返回 -1" << std::endl;

    std::cout << "scenario_r PASS (find_pet_with_soulmark：查询跟随上下场变化 + 存活判定)"
              << std::endl;
    return 0;
}
