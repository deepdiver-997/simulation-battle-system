// 场景 4810 —— K325 后手 PP-2+回已损失族（643，soul_lib/k325_second_pp2_heal.cpp）
//（2026-10-21，worktree ../agents/k325；号段 4810-4819 → T4810-T4813 四块）
//
// 断言口径（643「每回合若自身后出手则100%使对手所有PP降低2点，并恢复自身已损失
// 体力的1/2(boss无效)」）：
//   - T4810 **PP 全技能各 -2**：印主是**后手**（速度慢）→ 对手每个技能的 pp -2
//     （**全技能**判据：被削技能数 == 0 或 == 总数——K316 394 教训，用"任一"判据
//     会让"只降 1 个技能"的实现也通过）。
//   - T4811 **恢复已损失体力/2**：印主已损失 1000 体力 → 回合末恢复 500
//     （已损失 = max_hp - hp，K10 693 同款口径；精确断言）。
//   - T4812 **先手门闭**：印主是**先手**（速度快）→ PP 不降、体力不恢复。
//   - T4813 **满血不恢复**：印主满血（已损失 0）→ 体力不恢复（"恢复已损失"无对象）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {100,2,2,1}（不补 {0,0} 前缀）——实现侧
//   走 owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ **100% 概率 → 断言可精确**（本单比 394 好测：无随机性、不需要统计带）。
// ⚠️ 印主的最大体力 = make_pet 的 hp 参数（harness base[HP] = hp）；
//   压体力到 max-1000 → 已损失 1000 → 恢复 1000/2 = 500（精确）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：filler
constexpr int kMaxHp = 99999;
constexpr int kLostHp = 1000; // 压掉的血量

SoulMark make_mark() {
    // DB args 原序 [100,2,2,1]：db[0]=100（"100%"）、db[1]=2（"降低2点"）、
    // db[2]=2（"1/2"分母）、db[3]=1 记档。
    SoulMark sm(643, "K325 魂印", "test", EffectArgs(std::vector<int>{100, 2, 2, 1}));
    sm.monster_id = 2817;
    return sm;
}

ElfPet mark_pet(int speed) {
    ElfPet p = make_pet(1, "大地之神·卡修斯", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/speed, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/speed, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4810 PP 全技能各 -2（后手）══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100)),  // 印主慢 → 后手
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(1).skills) {
            s.pp = 10;
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        int cut_count = 0;
        int total = 0;
        bool all_at_8 = true;
        for (const Skills& s : ctx.getPet(1).skills) {
            ++total;
            if (s.pp < 10) {
                ++cut_count;
            }
            if (s.pp != 8) {
                all_at_8 = false;
            }
        }
        CHECK(cut_count == 0 || cut_count == total,
              "T4810 643：后手 → 对手**所有**技能 PP 各 -2（被削技能数 "
                  + std::to_string(cut_count) + "/" + std::to_string(total) + "）");
        CHECK(all_at_8,
              "T4810 643：每个技能的 PP 恰为 10-2=8（实测全部等于 8 = "
                  + std::to_string(all_at_8) + "）");
        std::cout << "  [T4810] PP 全技能各-2 通过\n";
    }

    // ══ T4811 恢复已损失体力/2 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp - kLostHp;   // 已损失 1000
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp - hp_before, kLostHp / 2,
                 "T4811 643：恢复自身已损失体力的 1/2（已损失 "
                     + std::to_string(kLostHp) + " → 恢复 " + std::to_string(kLostHp / 2)
                     + "，实测 " + std::to_string(ctx.getPet(0).hp - hp_before) + "）");
        std::cout << "  [T4811] 恢复已损失体力 通过\n";
    }

    // ══ T4812 先手门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/300)),  // 印主快 → 先手
                         make_party(foe_pet(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (Skills& s : ctx.getPet(1).skills) {
            s.pp = 10;
        }
        ctx.seerRobot[0].elfPets[0].hp = kMaxHp - kLostHp;
        const int hp_before = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        bool any_cut = false;
        for (const Skills& s : ctx.getPet(1).skills) {
            if (s.pp < 10) {
                any_cut = true;
            }
        }
        CHECK(!any_cut, "T4812 643：自身是**先手** → 门闭，对手 PP 不降");
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "T4812 643：自身是**先手** → 门闭，体力不恢复（实测 "
                     + std::to_string(ctx.getPet(0).hp - hp_before) + "）");
        std::cout << "  [T4812] 先手门闭对照 通过\n";
    }

    // ══ T4813 满血不恢复（已损失 0）══
    {
        ScenarioBattle b(make_party(mark_pet(/*speed=*/100)),
                         make_party(foe_pet(/*speed=*/300)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;   // 满血
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, hp_before,
                 "T4813 643：满血（已损失 0）→ 不恢复体力（实测 "
                     + std::to_string(ctx.getPet(0).hp - hp_before) + "）");
        std::cout << "  [T4813] 满血不恢复 通过\n";
    }

    std::cout << "scenario_4810 PASS (K325 后手 PP-2+回已损失族)" << std::endl;
    return 0;
}
