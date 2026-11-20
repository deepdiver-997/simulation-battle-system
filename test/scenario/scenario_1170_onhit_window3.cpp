// 场景 1170 —— J32 受击窗响应族三（2 宏，moves_lib/j_onhit_window3.cpp）
//（2026-10-02，worktree ../agents/j32；号段 1170-1179 → T1170-T1171 两块）
//
// 断言口径：
//   - 84 用统计带（50%×16 场 qualifying 受击，麻痹 25% 行动跳过偏差使 P(全未中)
//     ≈ 0.4^16 → 末态"已麻痹"断言成立概率 ≈1）：正例 16 场拍打后断麻痹在身；
//     负例 16 场属性技（修复）后断未麻痹（属性直伤不触发）；skip 场由驱动 NONE 承接；
//   - 427 用 ability_levels 精确断言（R1/R2 两次攻击叠到 -2、窗过期 R4 不再叠；
//     属性技不触发门）；
//   - 载体链排查：20475/26502 均单效果载体。
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

constexpr int kPaiDa = 10004;   // 拍打：物40（对手探针攻击）
constexpr int kLeiGuang = 20475; // 雷光铠甲[84]|[5,50]：属（受物理攻击50%麻痹，5回合）
constexpr int kQiMing = 26502;   // 启明祝愿[427]|[3,-1]：属（受直接攻击→双防-1，3回合）
constexpr int kXiuFu = 20309;    // 神经修复：属（对手属性技探针——不触发窗）
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);
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

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 100) {
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

    // ══ T1170 84 受物理攻击 50% 麻痹（雷光铠甲 [84]|[5,50]，统计带）══
    {
        {
            // 正例：5 局独立对照（每局窗 5 回合 × 50%）——P(全未中)=0.5^25≈3e-8
            int paralyzed_games = 0;
            for (int g = 0; g < 5; ++g) {
                ScenarioBattle b(
                    make_party(self_pet({kLeiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                    make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                for (int r = 0; r < 4; ++r) {   // R2..R5 窗内继续吃拍打
                    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                    b.drive();
                }
                if (ctx.has_active_abnormal_status(1, kParalysis)) {
                    ++paralyzed_games;
                }
            }
            CHECK(paralyzed_games >= 1,
                  "T1170.a 84：5 局×5 回合 50% 麻痹至少一局落地（" +
                      std::to_string(paralyzed_games) + "/5，P(全未)≈3e-8）");
        }
        {
            // 负例：窗内 5 场全属性技（属性直伤不触发）→ 全部未麻痹
            int paralyzed_games = 0;
            for (int g = 0; g < 5; ++g) {
                ScenarioBattle b(
                    make_party(self_pet({kLeiGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                    make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                for (int r = 0; r < 4; ++r) {
                    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                    b.drive();
                }
                if (ctx.has_active_abnormal_status(1, kParalysis)) {
                    ++paralyzed_games;
                }
            }
            CHECK(paralyzed_games == 0,
                  "T1170.b 84 门：属性技不触发（5 局全未麻痹，实测 " +
                      std::to_string(paralyzed_games) + " 局麻痹）");
        }
        std::cout << "  [T1170] 84 受击麻痹窗 通过\n";
    }

    // ══ T1171 427 受直接攻击→双防-1（启明祝愿 [427]|[3,-1]）══
    {
        ScenarioBattle b(make_party(self_pet({kQiMing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手拍打
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -1, "T1171.a 427：R1 防御 -1");
        CHECK_EQ(ctx.ability_levels[1][3], -1, "T1171.a 427：R1 特防 -1");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 再吃一刀
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -2, "T1171.b 427：每次直接攻击独立叠 -2");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 窗内末刀
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -3, "T1171.c 427：R3 窗内仍叠 -3");
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R4 窗过期
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][2], -3, "T1171.d 427：R4 窗过期不再叠");
        std::cout << "  [T1171] 427 受击双防叠降 通过\n";
    }

    std::cout << "scenario_1170 PASS (J32 受击窗响应族三 84/427)" << std::endl;
    return 0;
}
