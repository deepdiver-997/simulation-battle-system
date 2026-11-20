// 场景 1660 —— J76 提升镜像压制族（1 宏，moves_lib/j_mirror_suppress.cpp）
//（2026-10-03，worktree ../agents/j76；号段 1660-1669 → T1660 三块）
//
// 断言口径：
//   - 镜像 = 自身 +2 攻 → 对手槽0 -2（1:1 同幅度），其余槽 0、PP 不动；
//   - 未满足兜底 = 自身无正槽 → 对手随机 2 项 PP 归零（魔君相助 1245=[2]；归零槽
//     随机选取但**数量确定**——恰 2 个槽 pp==0，其余 >0）；
//   - 未触发兜底 = 对手槽0 预置 -6（stat_change 直写不查免弱）→ 镜像 stat_drop
//     AT_FLOOR 不算 SUCCESS → 全体未触发 → 随机 2 项 PP 归零、槽0 保持 -6；
//   - 载体魔君相助 25690 [191,47,1245]|[4,4,2]：属性技零伤害；链上 191/47=自侧
//     免反/免弱票（对断言面零干扰）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打
constexpr int kMoJun = 25690;   // 魔君相助[191,47,1245]|[4,4,2]：属性技（镜像压制/随机2项归零）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int count_zero_pp(BattleContext& ctx) {
    int n = 0;
    for (int i = 0; i < 5; ++i) {
        if (ctx.getPet(1).skills[i].pp == 0) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1660 1245 自身有提升 → 对手等量下降（魔君相助 1245=[2]）══
    {
        // a) 镜像路径：自身 +2 攻 → 对手槽0 -2，PP 全 >0
        {
            ScenarioBattle b(make_party(self_pet({kMoJun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS,
                  "T1660.a 前置：自身 +2 攻");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -2, "T1660.a 1245：对手攻槽镜像 -2");
            CHECK_EQ(ctx.ability_levels[1][1], 0, "T1660.a 1245：其余槽不动");
            CHECK_EQ(count_zero_pp(ctx), 0, "T1660.a 1245：镜像路径不触发 PP 归零");
        }
        // b) 未满足兜底：自身无正槽 → 随机 2 项 PP 归零
        {
            ScenarioBattle b(make_party(self_pet({kMoJun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_zero_pp(ctx), 2,
                     "T1660.b 1245：未满足 → 恰 2 项 PP 归零（实测 " +
                         std::to_string(count_zero_pp(ctx)) + "）");
            bool all_zero = true;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                if (ctx.ability_levels[1][i] != 0) {
                    all_zero = false;
                }
            }
            CHECK(all_zero, "T1660.b 1245：无镜像（六槽 0）");
        }
        // c) 未触发兜底：对手槽0 已 -6（AT_FLOOR）→ 随机 2 项 PP 归零、槽0 保持 -6
        {
            ScenarioBattle b(make_party(self_pet({kMoJun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            CHECK(stat_change(&ctx, 0, 0, 2) == StatChangeResult::SUCCESS,
                  "T1660.c 前置：自身 +2 攻");
            CHECK(stat_change(&ctx, 1, 0, -6) == StatChangeResult::SUCCESS,
                  "T1660.c 前置：对手攻槽 -6（到底）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -6, "T1660.c 1245：AT_FLOOR 未再降");
            CHECK_EQ(count_zero_pp(ctx), 2,
                     "T1660.c 1245：未触发 → 恰 2 项 PP 归零（实测 " +
                         std::to_string(count_zero_pp(ctx)) + "）");
        }
        std::cout << "  [T1660] 1245 提升镜像压制 通过\n";
    }

    std::cout << "scenario_1660 PASS (J76 提升镜像压制族 1245)" << std::endl;
    return 0;
}
