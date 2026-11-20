// 场景 2310 —— J136 暴击门族（1527/1465/461，moves_lib/j_crit_gates.cpp）
//（2026-10-04，worktree ../agents/j136；号段 2310-2319）
//
// 断言口径（暴击 = resolvedDamage.isCrit 旗 / ws.must_crit 直读；载体 crit_rate=0——
//           must_crit 是"必定"通道，0 率技能也要暴，58 圣光气口径）：
//   - 1527 后手必暴（蕴灵红壤 35149 特）：(a) 我速 50<300 后手 → 必暴（isCrit）；
//     (b) 我速 400 先手对照 → 不暴。
//   - 1465 先手 50% 必暴（激化胞质体 37938 [1]）：统计带——先手施放 20 场，
//     暴击场次 ∈ [4,16]（binomial p=0.5，±2.2σ）；必暴场对手无降、未暴场对手六槽 -1。
//   - 461 低体持续必暴（疾动旋风击 [3]）：hp<1/3 施放 → R2 起每回合攻击必暴；
//     hp≥1/3 对照 → 不暴。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打（填充/对照刀）
constexpr int kYunLing = 35149;   // 蕴灵红壤 [1527]（特）
constexpr int kJiHua  = 37938;    // 激化胞质体 [1465]|[1]（物）
constexpr int kJiDong = 13926;    // 疾动旋风击 [461]|[3]（物）
constexpr int kJiGuangDao = 19011; // 激光刀（foe 削血刀）

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "暴击门", skills, speed, atk, hp);
    accuracy_100(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, 999999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2310 1527 后手必暴 ══
    {
        {   // (a) 后手 → 必暴
            ScenarioBattle b(make_party(self_pet({kYunLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 50)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.resolvedDamage.isCrit, "T2310.a 1527 后手：必暴");
        }
        {   // (b) 先手对照 → 不暴
            ScenarioBattle b(make_party(self_pet({kYunLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 400)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.resolvedDamage.isCrit, "T2310.b 1527 先手对照：不暴");
        }
        std::cout << "  [T2310] 1527 后手必暴+对照 通过\n";
    }

    // ══ T2311 1465 先手 50% 必暴 / 未触发全属性降（统计带）══
    {
        int crits = 0;
        int drops = 0;
        for (int t = 0; t < 20; ++t) {
            ScenarioBattle b(make_party(self_pet({kJiHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 400)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (ctx.resolvedDamage.isCrit) {
                ++crits;
                CHECK(ctx.ability_levels[1][0] == 0,
                      "T2311 必暴场不应有降（slot0=" +
                          std::to_string(ctx.ability_levels[1][0]) + "）");
            } else {
                ++drops;
                int neg = 0;
                for (int i = 0; i < 6; ++i) {
                    if (ctx.ability_levels[1][i] == -1) {
                        ++neg;
                    }
                }
                CHECK_EQ(neg, 6, "T2311 未暴场：对手六槽 -1（实测 " + std::to_string(neg) + "）");
            }
        }
        CHECK(crits >= 4 && crits <= 16,
              "T2311 统计带：20 场暴击 " + std::to_string(crits) + " ∉ [4,16]");
        std::cout << "  [T2311] 1465 50% 必暴/全属性降统计带 通过（crits=" << crits
                  << "）\n";
        (void)drops;
    }

    // ══ T2312 461 低体持续必暴 ══
    {
        {   // (a) hp < 1/3 → R2 起必暴
            ScenarioBattle b(make_party(self_pet({kJiDong, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300,
                                                 /*atk=*/1000, /*hp=*/900)),
                             make_party(foe_pet({kJiGuangDao, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100,
                                                /*atk=*/1100)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1： foe 削我一刀（激光刀 atk=1100 → 红 ~604-672）→ 我 hp 228-296 < 300
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int low_hp = ctx.getPet(0).hp;
            CHECK(low_hp * 3 < 900, "T2312.a 前置：hp<1/3（实测 " + std::to_string(low_hp) +
                                        "）");
            // R2：我施放（低体门成立，开窗）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // R2 的当刀不暴（"从下回合开始"）；R3 起必暴
            CHECK(!ctx.resolvedDamage.isCrit, "T2312.a R2 当刀不暴（下回合起）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.resolvedDamage.isCrit, "T2312.a R3：窗内必暴");
        }
        {   // (b) hp 充足对照 → 不暴
            ScenarioBattle b(make_party(self_pet({kJiDong, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300,
                                                 /*atk=*/1000, /*hp=*/900)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 满血施放
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.resolvedDamage.isCrit, "T2312.b 满血对照：门不成立不暴");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.resolvedDamage.isCrit, "T2312.b 次回合仍不暴");
        }
        std::cout << "  [T2312] 461 低体持续必暴+对照 通过\n";
    }

    std::cout << "scenario_2310 ALL PASS" << std::endl;
    return 0;
}
