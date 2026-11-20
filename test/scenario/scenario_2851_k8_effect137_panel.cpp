// 场景 2851 —— K8 effect 137：圣甲家族 + 达尔妮面板（SOUL_PRE_BATTLE STAT_MODIFY）
//
// 断言口径：
//   A 圣甲地裂 2432（tips 体40 防16 特防16；5 参 args 布局噪声弃用）。
//   B 圣甲幻瞳 2478（tips 体20 特攻25 速12——args 丢速度 12，tips 补齐）。
//   C 达尔妮 1948：六维全部 +5。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

struct Probe {
    int atk, satk, def, sdef, speed, hp;
};

Probe probe_panel(int monster_id, std::vector<int> db_args) {
    ScenarioBattle b(scenario::make_party({scenario::make_pet(
        1, "持有方", {10005, 10005, 10005, 10005, 10005},
        /*speed=*/90, /*atk=*/120, /*hp=*/800, make_mark(137, std::move(db_args), monster_id))}),
        scenario::make_party(scenario::make_pet(2, "对手", {10005, 10005, 10005, 10005, 10005},
                                                100, 10, 500)));
    BattleContext& ctx = b.ctx();
    const ElfPet& p = ctx.getPet(0);
    return {p.numericalProperties[NumericalPropertyIndex::PHYSICAL_ATTACK],
            p.numericalProperties[NumericalPropertyIndex::SPECIAL_ATTACK],
            p.numericalProperties[NumericalPropertyIndex::DEFENSE],
            p.numericalProperties[NumericalPropertyIndex::SPECIAL_DEFENSE],
            p.numericalProperties[NumericalPropertyIndex::SPEED],
            p.numericalProperties[NumericalPropertyIndex::HP]};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 圣甲地裂
    {
        const Probe p = probe_panel(2432, {40, 0, 16, 0, 16});
        CHECK_EQ(p.def, 116, "A1 防御 +16");
        CHECK_EQ(p.sdef, 116, "A2 特防 +16");
        CHECK_EQ(p.hp, 840, "A3 体力 +40");
        CHECK_EQ(p.atk, 120, "A4 攻击不变");
    }
    // B 圣甲幻瞳（args 缺速度，tips 权威补齐）
    {
        const Probe p = probe_panel(2478, {20, 0, 0, 25, 0});
        CHECK_EQ(p.satk, 145, "B1 特攻 +25");
        CHECK_EQ(p.speed, 102, "B2 速度 +12（tips）");
        CHECK_EQ(p.hp, 820, "B3 体力 +20");
    }
    // C 达尔妮六维+5
    {
        const Probe p = probe_panel(1948, {5, 5, 5, 5, 5, 5});
        CHECK_EQ(p.atk, 125, "C1 物攻 +5");
        CHECK_EQ(p.satk, 125, "C2 特攻 +5");
        CHECK_EQ(p.def, 105, "C3 防御 +5");
        CHECK_EQ(p.sdef, 105, "C4 特防 +5");
        CHECK_EQ(p.speed, 95, "C5 速度 +5");
        CHECK_EQ(p.hp, 805, "C6 体力 +5");
    }

    std::cout << "scenario_2851 PASS (K8 effect137 圣甲/达尔妮面板)" << std::endl;
    return 0;
}
