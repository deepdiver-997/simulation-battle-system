#include <utils/sbs_rand.h>
// 场景 4880 —— K332 增长吸取族（688，soul_lib/k332_growing_absorb.cpp）
//（2026-10-21，worktree ../agents/k332；号段 4880-4889 → T4880 一块六断言）
//
// 断言口径（双方 max=8000000；印主 hp 起测 1000000；对手 NONE；FIXED 无浮动）：
//   R1 吸 1/9（888888）→ 双向等量；R2 吸 2/9（1777777）→ 双向；
//   R3 吸 3/9 达上限 1/3（2666666）→ **回血照常落地**；R4 封顶再吸 → 累计已超
//   自身 max → heal_impl 钳到满血（min(max_hp, hp+heal)）。
// ⚠️ 勘误记档（2026-10-21 溢出修复同步改）：本场景曾用 max=80000000 并断言
//   "R3/R4 单次回复 > max/4 被经济规则拒回"——该规则不存在，实为 heal_impl
//   int 乘法溢出被 <0 守卫整笔归零（26666666 > INT_MAX/100≈21474836）。内核
//   已改为响亮报错（乘积超 int 即 throw），场景改用 8M 量级、断言回血落地。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kMaxHp = 8000000;

SoulMark make_mark() {
    // DB args 原序 [9,9,3,1]：db[0]=9 分母、db[1]=9 步长分母、db[2]=3 上限分子。
    SoulMark sm(688, "K332 魂印", "test", EffectArgs(std::vector<int>{9, 9, 3, 1}));
    sm.monster_id = 2974;
    return sm;
}

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "仙化灵龙", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    sbs_srand(4880);
    ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    ctx.getPet(0).hp = 1000000;

    idle_round(b);   // R1：吸 1/9 = 888888
    CHECK_EQ(ctx.getPet(0).hp, 1000000 + 888888,
             "T4880 688：R1 吸收 1/9（自身实测 hp=" +
                 std::to_string(ctx.getPet(0).hp) + "）");
    CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 888888,
             "T4880 688：R1 对手 -888888（实测 " +
                 std::to_string(ctx.getPet(1).hp) + "）");
    idle_round(b);   // R2：2/9 = 1777777
    CHECK_EQ(ctx.getPet(0).hp, 1000000 + 888888 + 1777777,
             "T4880 688：R2 吸收 2/9（自身实测 hp=" +
                 std::to_string(ctx.getPet(0).hp) + "）");
    idle_round(b);   // R3：3/9 达 1/3 上限 → 吸取落、回血同额落地
    CHECK_EQ(ctx.getPet(0).hp, 1000000 + 888888 + 1777777 + 2666666,
             "T4880 688：R3 吸收 3/9 回血落地（溢出修复后，实测 hp=" +
                 std::to_string(ctx.getPet(0).hp) + "）");
    CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 888888 - 1777777 - 2666666,
             "T4880 688：R3 对手 -2666666（实测 " +
                 std::to_string(ctx.getPet(1).hp) + "）");
    idle_round(b);   // R4：封顶 3/9 → 累计吸取 1/9+2/9+3/9+3/9=1.0×max 已超自身 max
                     // → heal_impl 钳到满血（min(max_hp, hp+heal)，溢出修复保护路径）
    CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
             "T4880 688：R4 累计吸取钳到满血（实测 hp=" +
                 std::to_string(ctx.getPet(0).hp) + "）");
    CHECK_EQ(ctx.getPet(1).hp, kMaxHp - 888888 - 1777777 - 2666666 - 2666666,
             "T4880 688：R4 封顶吸取继续（实测对手 hp=" +
                 std::to_string(ctx.getPet(1).hp) + "）");

    std::cout << "scenario_4880 PASS (K332 增长吸取族)" << std::endl;
    return 0;
}
