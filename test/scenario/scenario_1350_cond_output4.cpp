// 场景 1350 —— J49 条件输出族四（814/415/476，moves_lib/j_cond_output4.cpp）
//（2026-10-02，worktree ../agents/j49；号段 1350-1359 → T1350-T1352 三块）
//
// 断言口径：
//   - 814 先手门双半（19068 飞雪落影歌 [40] 特150，速度序）：先手 → 对手掉
//     视图双防和×40% + 红；后手 → 只余红。视图双防运行时读。
//   - 415 阈值双带（14662 苍生无极光 [300,100] 特150，atk=1000 红 ~1346>300 →
//     +100 回；atk=1 红 ~2 → 无回）。自体缺口先削 2000。
//   - 476 后手门双半（17247 魔·吞天噬地 [300,100] 物160，速度序）：后手 → +300 回；
//     先手 → 不回（foe 恒先手、我满血钳——读 hp 差分）。
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

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（对手填充刀）
constexpr int kFeiXue  = 19068;   // 飞雪落影歌 [814]|[40] 特150（814 先手双防粉）
constexpr int kBianNian = 14662;  // 苍生无极光 [415]|[300,100] 特150（415 阈300 回100）
constexpr int kLingDong = 14136;  // 灵动破岩斩 [476]|[100] 物150（476 后手回100）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "条件四", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
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

    // ══ T1350 814 先手门双防粉（19068 [40]）══
    {
        int loss_first = 0;
        int loss_second = 0;
        {
            ScenarioBattle b(make_party(self_pet({kFeiXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我先手
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_first = 99999 - ctx.getPet(1).hp;
        }
        {
            ScenarioBattle b(make_party(self_pet({kFeiXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/1000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我后手
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            loss_second = 99999 - ctx.getPet(1).hp;
        }
        const int def_sum = 100 + 100;   // make_pet 防/特防恒 100（视图无等级变化）
        const int pink = def_sum * 40 / 100;
        // 红（atk=1000 p150）：0.84×10×150+2 = 1262 × (0.851~1.0) → [1074, 1262]。
        CHECK(loss_first >= pink + 1074 && loss_first <= pink + 1262,
              "T1350.a 814 先手：双防粉 " + std::to_string(pink) + "+红 1074~1262（实测掉 " +
                  std::to_string(loss_first) + "）");
        CHECK(loss_second >= 1074 && loss_second <= 1262,
              "T1350.b 814 后手门：只余红 1074~1262（实测掉 " +
                  std::to_string(loss_second) + "）");
        std::cout << "  [T1350] 814 先手双防粉+门 通过 (first=" << loss_first << " second="
                  << loss_second << ")\n";
    }

    // ══ T1351 415 高伤回血（14662 [300,100]）══
    {
        {   // (a) atk=1000：红 ~1346 > 300 → 回 100（先削自 2000 留缺口）
            ScenarioBattle b(make_party(self_pet({kBianNian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1000, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 削自留缺口
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            CHECK(hp_low < 4800 && hp_low > 2500, "T1351.a 前置：缺口 5000-hp_low>200");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 苍生无极光
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp - hp_low, 100,
                     "T1351.a 415：红>300 → 回 100（实测回 " +
                         std::to_string(ctx.getPet(0).hp - hp_low) + "）");
        }
        {   // (b) atk=1：红 ~2 ≤ 300 → 无回
            ScenarioBattle b(make_party(self_pet({kBianNian, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, hp_low, "T1351.b 415 门：红≤300 无回");
        }
        std::cout << "  [T1351] 415 高伤回血+门 通过\n";
    }

    // ══ T1352 476 后手回血（17247 [300,100]）══
    {
        {   // (a) 后手：+300（先削自 2000 留缺口）
            ScenarioBattle b(make_party(self_pet({kLingDong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挨刀（对手先手）
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 我后手出 476
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp - hp_low, 100,
                     "T1352.a 476：后手回 100（实测回 " +
                         std::to_string(ctx.getPet(0).hp - hp_low) + "）");
        }
        {   // (b) 先手对照：不回
            ScenarioBattle b(make_party(self_pet({kLingDong, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/3000)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int hp_low = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 我先手出 476
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.getPet(0).hp <= hp_low, "T1352.b 476 门：先手不回（hp 只降不升）");
        }
        std::cout << "  [T1352] 476 后手回血+门 通过\n";
    }

    std::cout << "scenario_1350 PASS (J49 条件输出族四)" << std::endl;
    return 0;
}
