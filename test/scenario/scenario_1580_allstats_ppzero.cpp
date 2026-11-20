// 场景 1580 —— J71 全属性/PP归零族（1145/912，moves_lib/j_allstats_ppzero.cpp）
//（2026-10-03，worktree ../agents/j71；号段 1580-1589 → T1580-T1581 两块）
//
// 断言口径：
//   - 1145 双方全属性+1（37293 [1145,843]|[1,2,2]，843=自身先制良性）：出手后双方
//     六槽全 == +1（stat_change 真原语，全确定）。
//   - 912 负槽门随机 PP 归零（28377 [3] 属性技）：对手有负槽 → 恰 3 槽 PP==0、
//     其余 35（随机选槽非确定 → 计数断言）；对照无负槽 → 全 35 不动。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa  = 10004;   // 拍打（填充刀）
constexpr int kQuanSheng = 37293;  // [1145,843]|[1,2,2] 物150（1145 双方全属性+1）
constexpr int kJinLing = 28377;  // 噤灵食心 [912]|[3] 属性技（912 随机 3 项 PP 归零）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "全属族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1580 1145 双方全属性+1（37293 [1]）══
    {
        ScenarioBattle b(
            make_party(self_pet({kQuanSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        for (int side = 0; side <= 1; ++side) {
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(ctx.ability_levels[side][i], 1,
                         "T1580 1145：侧 " + std::to_string(side) + " 槽 " +
                             std::to_string(i) + " == +1");
            }
        }
        std::cout << "  [T1580] 1145 双方全属性+1 通过\n";
    }

    // ══ T1581 912 负槽门随机 PP 归零（28377 [3]）══
    {
        {   // (a) 对手有负槽 → 恰 3 槽 PP==0、其余 35
            ScenarioBattle b(
                make_party(self_pet({kJinLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, -1) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int zeroed = 0;
            int full = 0;
            for (int i = 0; i < 5; ++i) {
                const int pp = ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp;
                if (pp == 0) {
                    ++zeroed;
                } else if (pp == 35) {
                    ++full;
                }
            }
            CHECK_EQ(zeroed, 3, "T1581.a 912：恰 3 槽归零（实测 " + std::to_string(zeroed) +
                                    "）");
            CHECK_EQ(full, 2, "T1581.a 912：其余 2 槽不受影响");
        }
        {   // (b) 对照：无负槽 → 全 35 不动
            ScenarioBattle b(
                make_party(self_pet({kJinLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            int full = 0;
            for (int i = 0; i < 5; ++i) {
                if (ctx.getPet(1).skills[static_cast<std::size_t>(i)].pp == 35) {
                    ++full;
                }
            }
            CHECK_EQ(full, 5, "T1581.b 912 门：无负槽 PP 全不动");
        }
        std::cout << "  [T1581] 912 负槽门随机 PP 归零 通过\n";
    }

    std::cout << "scenario_1580 PASS (J71 全属性/PP归零族)" << std::endl;
    return 0;
}
