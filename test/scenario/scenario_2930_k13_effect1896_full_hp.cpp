#include <utils/sbs_rand.h>
// 场景 2930 —— K13 effect 1896 满体分支（图托尔斯 4311 [3,1,50,200,200,2,100,1]）
//
// 断言口径（持有方开局满体 → ROUND_START 走满体分支）：
//   A 对手 5 技 PP 合计 25 → 22（任意 3 项各 -1，srand 定哪三项、合计确定）。
//   B ws.damage_add_pct[0]==50（当回合攻伤窗字段）。
//   C 行为：伤害抬升（⚠️ 只做方向断言——addPct 在公式内部取整，外部 ×1.5 差 ±1%
//     首跑实证 2155 vs 2166；机制精确性由字段断言 B 钉住）。
#include <cstdlib>
#include <iostream>

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

ElfPet holder_pet(bool with_mark) {
    ElfPet p = scenario::make_pet(1, "持有方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
                                  with_mark ? make_mark(1896, {3, 1, 50, 200, 200, 2, 100, 1}, 4311)
                                            : SoulMark{});
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}
ElfPet dummy_pet() {
    return scenario::make_pet(2, "木桩", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              /*speed=*/50, /*atk=*/10, /*hp=*/1000000);
}

// 持有方满体出手一回合；可选读 damage_add_pct 窗。
struct Probe {
    int dummy_loss = 0;
    int pp_lost = 0;
    int amp_pct = 0;
};

Probe probe_round(bool with_mark) {
    ScenarioBattle b(scenario::make_party(holder_pet(with_mark)),
                     scenario::make_party(dummy_pet()));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261004);
    b.start();
    const int dummy_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    int pp0 = 0;
    for (const Skills& s : ctx.seerRobot[1].elfPets[ctx.on_stage[1]].skills) {
        pp0 += s.pp;
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    int pp1 = 0;
    for (const Skills& s : ctx.seerRobot[1].elfPets[ctx.on_stage[1]].skills) {
        pp1 += s.pp;
    }
    return {dummy_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp,
            pp0 - pp1,
            static_cast<int>(ctx.ws.damage_add_pct[0])};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const Probe marked = probe_round(true);
    const Probe base = probe_round(false);

    CHECK_EQ(marked.pp_lost, 3, "A 对手 3 项 PP 各 -1（合计确定）");
    CHECK_EQ(marked.amp_pct, 50, "B 当回合攻伤窗 +50");
    CHECK(marked.dummy_loss > 1000, "伤害量级合理");
    CHECK(marked.dummy_loss > base.dummy_loss, "C 攻伤窗生效（伤害抬升）");
    CHECK_EQ(base.pp_lost, 0, "C2 对照无印记 → PP 不动");

    std::cout << "scenario_2930 PASS (K13 effect1896 满体分支)" << std::endl;
    return 0;
}
