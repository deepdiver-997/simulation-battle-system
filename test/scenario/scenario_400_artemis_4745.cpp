#include <utils/sbs_rand.h>
// 场景 400：阿尔忒弥斯 4745 五技 挽风游天歌（2275/2279）
// （2026-09-28 night-3 落地 T1/T2；2026-09-29 D24 复工补 T3 击杀正路径）
// （实现 resources/moves_lib/d5_D24.cpp）
//
// T1 2275：未击败 → 对手在场宠被标记为狩杀之的（键值断言）。
// T2 2279 负例：对手存活（无击败）→ 击杀探针不触发、无下次攻击增伤授予。
// T3 2279 正路径：R1 挽风标记 → R2 挽风灭杀被标者（2279 探针由本技能使用时注册、
//    当回合有效，击杀必须出自挽风本体）→ 死亡漏斗 hp 钳 0 进 CHOOSE_AFTER_DEATH、
//    补位后转标、每魂(2层)下次攻击 +120% 授予并在下次攻击后消费。
//    （night-3 曾报"击杀正路径被 ChooseAfterDeath 自旋阻断"——复核结论：自旋 =
//    CHOOSE_AFTER_DEATH 停留期间未提交补位输入（harness wait_for_input 为空转 no-op），
//    或死宠 hp≠0 时 need_input 恒假、handle 拿空 buffer 重入；攻击致死走 deal_damage
//    钳 0，场景显式 submit_operation(CHOOSE_PET) 补位即不触雷，内核无需改动。）
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;       // 拍打
constexpr int kWanFeng = 37864;   // 挽风游天歌：[1747,1600,2275,2279]
constexpr int kMarkSideKey = 990474511;
constexpr int kMarkSlotKey = 990474512;
constexpr int kHuntSoulKey = 990474501;

ElfPet artemis(int atk = 20) {
    ElfPet p = scenario::make_pet(4745, "阿尔忒弥斯", {kWanFeng, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, 100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = scenario::make_pet(1, "守方", {kHit, kHit, kHit, kHit, kHit},
                                  /*speed=*/100, /*atk=*/0, 100000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 2275：未击败 → 标记狩杀之的 ==========
    {
        ScenarioBattle b(scenario::make_party(artemis()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);
        auto& st = ctx.seerRobot[0].elfPets[0].soulmark_storage;
        CHECK(st.find(kMarkSideKey) != st.end()
                  && std::any_cast<int>(st[kMarkSideKey]) == 1,
              "T1 标记 side = 对手方");
        CHECK(st.find(kMarkSlotKey) != st.end()
                  && std::any_cast<int>(st[kMarkSlotKey]) == ctx.on_stage[1],
              "T1 标记 slot = 对手在场槽");
        std::cout << "  [T1] 2275 标记狩杀之的 通过\n";
    }

    //========== T2 2279 负例：无击败 → 不授予 ==========
    {
        ScenarioBattle b(scenario::make_party(artemis()), scenario::make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b);
        CHECK(ctx.attack_boost_grants[0].empty(),
              "T2 无击败：2279 不授予下次攻击增伤");
        std::cout << "  [T2] 2279 无击败不授予 通过\n";
    }

    //========== T3 2279 正路径：击败狩杀之的 → 转标 + 每魂下次攻击 +60% ==========
    {
        sbs_srand(20260929);
        const ElfPet a = artemis(/*atk=*/3000);
        const ElfPet weak = foe_pet();   // hp 100000：R1 吃住挽风不被击倒
        std::array<ElfPet, 6> foes{weak, weak, weak, weak, weak, weak};
        ScenarioBattle b(scenario::make_party(a), foes);
        b.start();
        BattleContext& ctx = b.ctx();

        // R1：挽风命中 → 2275 未击败 → 标记在场守方（T3 前置）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        {
            auto& st = ctx.seerRobot[0].elfPets[0].soulmark_storage;
            CHECK(st.find(kMarkSlotKey) != st.end()
                      && std::any_cast<int>(st[kMarkSlotKey]) == ctx.on_stage[1],
                  "T3 前置：R1 已标记对手在场宠");
        }
        // 预置 2 层狩杀之魂（魂印 2354 计层框架不属本批，直接落键）
        ctx.seerRobot[0].elfPets[0].soulmark_storage[kHuntSoulKey] = 2;

        // R2：压低守方体力 → 挽风灭杀被标者（伤害出自挽风本体，2279 探针同回合有效）
        ctx.getPet(1).hp = 50;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        REQUIRE(ctx.currentState == State::CHOOSE_AFTER_DEATH);
        CHECK_EQ(ctx.getPet(1).hp, 0, "T3：被击杀者体力钳 0（need_input 等待前提）");
        CHECK(ctx.attack_boost_grants[0].size() == 1
                  && ctx.attack_boost_grants[0][0].source_effect_id == 2279
                  && ctx.attack_boost_grants[0][0].remaining == 1
                  && ctx.attack_boost_grants[0][0].pct == 120,
              "T3：击败被标者 → 每魂(2 层)下次攻击 +60% 授予（2×60=120）");

        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);   // 补位下只
        {
            auto& st = ctx.seerRobot[0].elfPets[0].soulmark_storage;
            CHECK_EQ(std::any_cast<int>(st[kMarkSlotKey]), 1,
                     "T3：对方下只出战 → 狩杀之的转标到补位宠");
        }
        // R3：拍打（弱攻）→ 消费"下次攻击"授予
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.attack_boost_grants[0].empty(),
              "T3：下次攻击后授予消费（用后即清）");
        std::cout << "  [T3] 击败狩杀之的 → 转标 + 每魂下次攻击 +60% 通过\n";
    }

    //========== T4 2279 零魂分支：击败被标者但无狩杀之魂 → 只转标不授予 ==========
    {
        sbs_srand(20260929);
        const ElfPet a = artemis(/*atk=*/3000);
        const ElfPet weak = foe_pet();
        std::array<ElfPet, 6> foes{weak, weak, weak, weak, weak, weak};
        ScenarioBattle b(scenario::make_party(a), foes);
        b.start();
        BattleContext& ctx = b.ctx();
        // R1 标记；不落狩杀之魂（合成宠无 2354 框架，天然 0 层）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        ctx.getPet(1).hp = 50;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        REQUIRE(ctx.currentState == State::CHOOSE_AFTER_DEATH);
        CHECK(ctx.attack_boost_grants[0].empty(),
              "T4：0 层狩杀之魂 → 击败被标者也不授予增伤");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        {
            auto& st = ctx.seerRobot[0].elfPets[0].soulmark_storage;
            CHECK_EQ(std::any_cast<int>(st[kMarkSlotKey]), 1,
                     "T4：转标与魂层数无关，补位宠照常被标");
        }
        std::cout << "  [T4] 零魂只转标不授予 通过\n";
    }

    std::cout << "scenario_400 all passed" << std::endl;
    return 0;
}
