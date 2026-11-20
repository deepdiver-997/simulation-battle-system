#include <utils/sbs_rand.h>
// 场景 954 —— K3 记档子句回归：183（弹能力下降）不硬编 + 1057 闪避/必暴子句不误触发
//
// 183 神·阿波罗"反弹对手给予的能力下降"：无施加拦截钩子，按工单记档（内核微任务）——
//   本场景守住"没被硬编成半吊子行为"：被降级照常掉、不产生额外弹回。
// 1057 闪避/必暴子句记档：攻击流程不崩、伤害正常落地（未挂 dodge_rate/must_crit）。
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

    // ══ A 183 记档：被降级照常落地（不弹回、不误免）═════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "神·阿波罗", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(183, {}, 2681));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "进攻方", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/5000, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        stat_drop(&ctx, 0, /*防御槽=*/2, /*delta=*/1, /*actor=*/1);
        CHECK_EQ(ctx.ability_levels[0][2], -1, "A 降级照常落地（183 记档未硬编）");
        CHECK_EQ(ctx.ability_levels[1][2], 0, "A 对手不受弹回（无半吊子行为）");
    }
    // ══ B 1057 记档子句：攻击正常落地、流程不崩 ═════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "赤渊之蛇", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/100, /*atk=*/5000, /*hp=*/1000000,
                                           make_mark(1057, {250, 70, 1}, 3458));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/300, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20260916);
        b.start();
        const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp > 0,
              "B 1057 载体攻击正常（记档子句不影响主流程）");
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::OPERATION_CHOOSE_SKILL_MEDICAMENT),
                 "B 对局推进正常");
    }

    std::cout << "scenario_954 PASS (K3 记档子句：183 不硬编 + 1057 主流程无损)" << std::endl;
    return 0;
}
