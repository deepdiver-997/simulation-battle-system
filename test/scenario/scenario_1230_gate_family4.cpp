// 场景 1230 —— J38 未击败/击败门族四（3 宏，moves_lib/j_gate_family4.cpp）
//（2026-10-02，worktree ../agents/j38；号段 1230-1239 → T1230-T1232 三块）
//
// 断言口径：
//   - 1107 用 12 局独立对照统计带（50%/50% 独立双段）：自身 +6（六槽和）至少
//     3 局、对手 -6 至少 3 局——P(≤2 次)=二项(12,0.5) 累积 ≈1.9% 双侧各自独立；
//   - 678 用 ability_levels 精确断言（R1 挂门 → R2/R3/R4 每回合攻击 +1 → 累计 3，
//     R5 不再涨；击败门负例不涨）；
//   - 669 用击败门 + preemptive_level 断言（R1 击杀 → R2 MOVE_RIGHT 写 +1；
//     存活门负例不写）；
//   - 载体链排查：30425（797 消回无回合类=不触发）、19606（180 零参无害）、
//     32211（990 消强对无提升对手=不触发）。
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

constexpr int kPaiDa = 10004;   // 拍打：物40
constexpr int kHuPi  = 30425;   // 虎貔震天威[797,1107]|[3,50,1,50,1]：物160（双段50%）
constexpr int kMiLuan = 19606;  // 谜乱无心斩[180,678]|[3,0,1]：物160（未击败→下3回合攻+1）
constexpr int kMeiYing = 32211; // 魅影神舞圈[990,678,669]：物160（击败→下回合先制+1）
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

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = kMaxHp) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int slot_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1230 1107 未击败→双段 50% 升降（虎貔震天威，12 局统计带）══
    {
        int boost_games = 0, drop_games = 0;
        for (int g = 0; g < 12; ++g) {
            ScenarioBattle b(
                make_party(self_pet({kHuPi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            // 每局固定不同种子，避免独立局反复采样同一随机序列。
            b.ctx().seed_rng(1000U + static_cast<unsigned>(g) * 7919U);
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (slot_sum(ctx, 0) == 6) {
                ++boost_games;
            }
            if (slot_sum(ctx, 1) == -6) {
                ++drop_games;
            }
        }
        CHECK(boost_games >= 3,
              "T1230.a 1107：自身全属性+1 段至少 3/12 局（实测 " +
                  std::to_string(boost_games) + "，P(≤2)≈19% 双段独立单侧）");
        CHECK(drop_games >= 3,
              "T1230.b 1107：对手全属性-1 段至少 3/12 局（实测 " +
                  std::to_string(drop_games) + "）");
        std::cout << "  [T1230] 1107 双段概率升降 通过 (+" << boost_games << "/-"
                  << drop_games << " /12)\n";
    }

    // ══ T1231 678 未击败→下3回合每回合攻击+1（谜乱无心斩）══
    {
        // a) 正例：R1 攻击（对手存活）→ R2/R3/R4 每回合攻 +1 → 累计 3，R5 停
        {
            ScenarioBattle b(
                make_party(self_pet({kMiLuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(1).hp > 0, "T1231.a 前置：对手存活（未击败）");
            for (int r = 0; r < 3; ++r) {   // R2..R4 窗内（NextRounds 3）
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                CHECK_EQ(ctx.ability_levels[0][0], r + 1,
                         "T1231.a 678：R" + std::to_string(r + 2) + " 攻击 +1（累计 " +
                             std::to_string(r + 1) + "）");
            }
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 3, "T1231.a 678：R5 窗过期不再涨");
        }
        // b) 门负例：击杀（hp=1）→ 不武装
        {
            ScenarioBattle b(
                make_party(self_pet({kMiLuan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/1, /*hp=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T1231.b 678 门：击败对手不触发");
        }
        std::cout << "  [T1231] 678 未击败每回合强化 通过\n";
    }

    // ══ T1232 669 击败→下回合先制+1（魅影神舞圈，零参）══
    {
        // a) 正例：R1 击杀（两宠队伍死切驱动，953 协议）→ R2 preemptive_level +1
        {
            ElfPet fodder = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100, /*atk=*/1, /*hp=*/1);
            ElfPet mate = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{fodder, mate, mate, mate, mate, mate};
            ScenarioBattle b(
                make_party(self_pet({kMeiYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(static_cast<int>(ctx.currentState),
                     static_cast<int>(State::CHOOSE_AFTER_DEATH), "T1232.a 前置：停在死切");
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R2：MOVE_RIGHT tick
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.ws.preemptive_level[0] >= 1,
                  "T1232.a 669：R2 攻击先制 +1（level=" +
                      std::to_string(ctx.ws.preemptive_level[0]) + "）");
        }
        // b) 门负例：对手存活 → 不写
        {
            ScenarioBattle b(
                make_party(self_pet({kMeiYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.ws.preemptive_level[0] == 0, "T1232.b 669 门：未击败不写先制");
        }
        std::cout << "  [T1232] 669 击败先制 通过\n";
    }

    std::cout << "scenario_1230 PASS (J38 未击败/击败门族四 1107/678/669)" << std::endl;
    return 0;
}
