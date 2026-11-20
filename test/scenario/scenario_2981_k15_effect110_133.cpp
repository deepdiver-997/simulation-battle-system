#include <utils/sbs_rand.h>
// 场景 2981 —— K15 effect 110：能力下降反弹 + 133：致命免疫反弹
//
// 断言口径：
//   A 110：对持有方 stat_drop(-2) → 持有方与对手攻击等级同 -2（同槽同幅）。
//   B 133：对手必暴攻击 → 持有方零掉血（免疫）且攻方掉血 = 裸局必暴伤害（反弹）。
//   C 133 非暴：普通攻击照常落地（isCrit 门）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

ElfPet crit_attacker_pet() {
    ElfPet p = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/100000);
    scenario::set_crit_sixteenths(p, 16);   // 必暴
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

    // A 110 降转反弹
    {
        ScenarioBattle b(
            scenario::make_party({scenario::make_pet(1, "持有方",
                                                     {10005, 10005, 10005, 10005, 10005},
                                                     100, 10, 1000000, make_mark(110, {}, 2681))}),
            scenario::make_party(scenario::make_pet(2, "对手", {10005, 10005, 10005, 10005, 10005},
                                                    100, 10, 1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        (void)stat_drop(&ctx, 0, /*攻击槽=*/0, 2, 1);
        CHECK_EQ(ctx.ability_levels[0][0], -2, "A1 持有方被降 -2");
        // watcher 交付推迟（K9 勘误）→ 驱动一个状态冲交付后再断言反弹
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -2, "A2 对手同槽同幅 -2（反弹，交付后）");
    }

    // B/C 133 致命免疫反弹 + 非暴门
    {
        const int raw_crit = [&] {
            ScenarioBattle b(scenario::make_party(
                                 scenario::make_pet(1, "裸守方",
                                                    {10005, 10005, 10005, 10005, 10005},
                                                    100, 10, 1000000)),
                             scenario::make_party(crit_attacker_pet()));
            BattleContext& ctx = b.ctx();
            sbs_srand(20261004);
            b.start();
            const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            return hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        }();
        ScenarioBattle b(scenario::make_party(
                             scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                                                100, 10, 1000000, make_mark(133, {50, 19}, 2736))),
                         scenario::make_party(crit_attacker_pet()));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        const int holder_hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        const int atk_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp, holder_hp0,
                 "B1 必暴 → 免疫（零掉血）");
        CHECK_EQ(atk_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp, raw_crit,
                 "B2 攻方被反弹等量必暴伤");
    }
    // C 非暴门
    {
        ElfPet atk = scenario::make_pet(2, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                        300, 5000, 100000);
        for (Skills& s : atk.skills) {
            s.must_hit = 0;
            s.accuracy = 100;
        }
        ScenarioBattle b(scenario::make_party(
                             scenario::make_pet(1, "持有方", {10005, 10005, 10005, 10005, 10005},
                                                100, 10, 1000000, make_mark(133, {50, 19}, 2736))),
                         scenario::make_party(atk));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261004);
        b.start();
        const int hp0 = ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(hp0 - ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp > 0, "C 非暴 → 照常受伤");
    }

    std::cout << "scenario_2981 PASS (K15 effect110/133)" << std::endl;
    return 0;
}
