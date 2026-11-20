#include <utils/sbs_rand.h>
// 场景 959 —— K4 受伤阈值回满 + 每回合双防（effect 612 萨瑞罗斯）
//
// args[350,100,delta,1]：受伤>350（链首快照，犀牛口径）→ 恢复 100% 体力；
// 每回合双防+delta（308 行 delta=1 / 伙伴行 delta=2）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中
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

    // ══ A 每回合双防+1（普通行）═════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "萨瑞罗斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(612, {350, 100, 1, 1}, 2856))),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][2], 1, "A 防御 +1");
        CHECK_EQ(ctx.ability_levels[0][3], 1, "A 特防 +1");
    }
    // ══ B 受伤 > 350 → 回满 ═════════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "萨瑞罗斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(612, {350, 100, 1, 1}, 2856))),
                         scenario::make_party(scenario::make_pet(
                             2, "重炮", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, hp0,
                 "B 受伤>350 → 恢复全部体力（落血被回满抵平）");
    }
    // ══ B2 对照：无魂印同配置 → 落血远超 350（证明 B 的抵平来自回满）═════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "裸桩", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)),
                         scenario::make_party(scenario::make_pet(
                             2, "重炮", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp > 350,
              "B2 对照：同配置无魂印落血>350（阈值判定真发生了）");
    }
    // ══ C 受伤 ≤ 350 → 不回满 ═══════════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(scenario::make_pet(
                             1, "萨瑞罗斯", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                             make_mark(612, {350, 100, 1, 1}, 2856))),
                         scenario::make_party(scenario::make_pet(
                             2, "挠痒", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/1, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int loss = hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        CHECK(loss > 0 && loss <= 350, "C 前置：落血在阈值内");
        CHECK(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp < hp0,
              "C 受伤≤350 → 不回满");
    }

    std::cout << "scenario_959 PASS (K4 612 阈值回满 + 每回合双防)" << std::endl;
    return 0;
}
