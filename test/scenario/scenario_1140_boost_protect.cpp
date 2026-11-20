// 场景 1140 —— J29 强化保护/无强化先制族（2 宏，moves_lib/j_boost_protect.cpp）
//（2026-10-02，worktree ../agents/j29；号段 1140-1149 → T1140-T1141 两块）
//
// 断言口径：
//   - 1683 用原语直呼断言（clear_stat_boosts 返回 0=被 STAT_CLEAR 票挡、提升原值
//     保持；transfer_stat_boosts 返回 0=被 STAT_BOOST 票挡；窗过期后 clear 恢复 1）；
//   - 1860 用场景 001 条件先制翻盘法（自身速度 100 vs 对手 300：无提升时先制+1
//     翻盘先手 + preemptive_level 断言；有提升对照组不触发）——载体天奔零式
//     （链上 423 未实现=天然无噪声）；
//   - 1860 走 skills.cpp 选择期路由（2026-10-02 加 1860 一行，2298/779 同款）。
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

constexpr int kPaiDa  = 10004;   // 拍打：物40
constexpr int kGuHua  = 28755;   // 固化力场[1683,1684]|[3,3]：属（3回合保护）
constexpr int kLingShi = 36133;  // 天奔零式[1860,423]：属（无提升→先制+1必中，零参）
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1140 1683 提升保护窗（固化力场 [1683]|[3]）══
    {
        ScenarioBattle b(make_party(self_pet({kGuHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂保护
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        (void)stat_change(&ctx, 0, 0, 2);   // 自身有提升可被清
        CHECK_EQ(clear_stat_boosts(&ctx, 0), 0,
                 "T1140.a 1683：消除被 STAT_CLEAR 票挡（返回 0）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T1140.a 1683：提升原值保持");
        CHECK_EQ(transfer_stat_boosts(&ctx, 0, 1), 0,
                 "T1140.b 1683：吸取被 STAT_BOOST 票挡（返回 0）");
        CHECK_EQ(ctx.ability_levels[0][0], 2, "T1140.b 1683：吸取后提升仍保持");
        // 窗 3 回合：R2/R3 过后 R4 恢复（drive 空转两回合）
        for (int r = 0; r < 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
        }
        CHECK_EQ(clear_stat_boosts(&ctx, 0), 1, "T1140.c 1683：窗过期消除恢复（返回 1）");
        std::cout << "  [T1140] 1683 提升保护窗 通过\n";
    }

    // ══ T1141 1860 无提升→先制+1且必中（天奔零式，零参选择期路由）══
    {
        // a) 主场景：自身无提升、速度 100 vs 对手 300 → 先制+1 翻盘先手
        {
            ScenarioBattle b(
                make_party(self_pet({kLingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
                  "T1141.a 1860：无提升 → 低速自身先制+1 翻盘先手");
            CHECK(ctx.ws.preemptive_level[0] >= 1,
                  "T1141.a 1860：preemptive_level[0]=" +
                      std::to_string(ctx.ws.preemptive_level[0]));
        }
        // b) 对照组：自身有提升 → 不触发 → 高速对手先手
        {
            ScenarioBattle b(
                make_party(self_pet({kLingShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*speed=*/100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                ctx.ability_levels[0][i] = 1;   // 自身满提升（开战清零后预置）
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
                  "T1141.b 1860 门：有提升不触发（高速对手先手）");
        }
        std::cout << "  [T1141] 1860 无提升先制必中 通过\n";
    }

    std::cout << "scenario_1140 PASS (J29 强化保护/无强化先制族 1683/1860)" << std::endl;
    return 0;
}
