#include <utils/sbs_rand.h>
// 场景 957 —— K4 每回合能力对·(delta,slot) 布局（effect 641 爆炎莱茵）
//
// args "2 1 0 1 4 1" = count=2 + (delta,slot)×2 → (1,0)=攻+1、(1,4)=速+1
// （与 636 的 (slot,delta) 相反——行≠宏：同族不同宏布局不同，唯一解锚定见实现头注）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;    // 高速移动：属性技

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 回合 1：攻+1 速+1 ═════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "爆炎莱茵", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(641, {2, 1, 0, 1, 4, 1}, 2899))),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 1, "A 攻击 +1（(delta,slot) 布局）");
        CHECK_EQ(ctx.ability_levels[0][4], 1, "A 速度 +1");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "A 特攻不动（布局方向校验）");
    }

    std::cout << "scenario_957 PASS (K4 641 (delta,slot) 布局)" << std::endl;
    return 0;
}
