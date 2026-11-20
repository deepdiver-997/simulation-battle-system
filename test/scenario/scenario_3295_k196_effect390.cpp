#include <utils/sbs_rand.h>
// 场景 3295 —— K196 effect 390（每回合能力升降族）：五行 monster_table 行文驱动
//
//   A 天煞 2815：r1 特攻+1 速+1，r2 累计 +2（等级槽 1/4）。
//   B 末日天卫 3064：r1 双防+2（槽 2/3）。
//   C 梦魔 3113：对手速-1 命中-1（槽 4/5，stat_drop 通道）。
//   D 绝煞 2823：双防+1（槽 2/3）——五行全 MoNSTER 路由覆盖。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技（无伤害干扰）

SoulMark make_mark(int monster_id) {
    SoulMark sm(390, "K390", "test", EffectArgs());
    sm.monster_id = monster_id;
    return sm;
}

// 带 mark 跑 rounds 回合（双方 NONE），返回 0/1 方等级槽快照引用值。
int probe_level(int monster_id, int side, int slot, int rounds) {
    ElfPet holder = scenario::make_pet(1, "印主", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                       make_mark(monster_id));
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return ctx.ability_levels[side][slot];
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 天煞 2815：特攻+1 速+1，两回合累计 +2 ═══════════════════════════
    CHECK_EQ(probe_level(2815, 0, 1, 1), 1, "A1 天煞 r1 特攻+1");
    CHECK_EQ(probe_level(2815, 0, 4, 1), 1, "A2 天煞 r1 速+1");
    CHECK_EQ(probe_level(2815, 0, 1, 2), 2, "A3 天煞 r2 特攻累计+2");

    // ══ B 末日天卫 3064：双防+2 ═══════════════════════════════════════════
    CHECK_EQ(probe_level(3064, 0, 2, 1), 2, "B1 天卫 r1 防+2");
    CHECK_EQ(probe_level(3064, 0, 3, 1), 2, "B2 天卫 r1 特防+2");

    // ══ C 梦魔 3113：对手速-1 命中-1 ══════════════════════════════════════
    CHECK_EQ(probe_level(3113, 1, 4, 1), -1, "C1 梦魔对手速-1");
    CHECK_EQ(probe_level(3113, 1, 5, 1), -1, "C2 梦魔对手命中-1");

    // ══ D 绝煞 2823：双防+1 ═══════════════════════════════════════════════
    CHECK_EQ(probe_level(2823, 0, 2, 1), 1, "D1 绝煞 r1 防+1");
    CHECK_EQ(probe_level(2823, 0, 3, 1), 1, "D2 绝煞 r1 特防+1");

    // ══ E 鲁伊斯 2914：攻+1 速+1 ══════════════════════════════════════════
    CHECK_EQ(probe_level(2914, 0, 0, 1), 1, "E1 鲁伊斯 r1 攻+1");
    CHECK_EQ(probe_level(2914, 0, 4, 1), 1, "E2 鲁伊斯 r1 速+1");

    std::cout << "scenario_3295 PASS (K196 390 每回合能力升降五行)" << std::endl;
    return 0;
}
