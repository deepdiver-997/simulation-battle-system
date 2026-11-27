#include <utils/sbs_rand.h>
// 场景 2871 —— K10 effect 693：双节点程序（攻击技附已损%粉伤 / 攻·属技回血）
//
// 断言口径（srand 对齐 → 粉伤整数精确；粉伤不受 85~100% 浮动影响——浮动只在红伤公式）：
//   A 2992 [1,50,2,200]：攻击技 → 木桩多掉 已损×50%（持有方压 500 体 → 已损≈999500）。
//   B 2992 属性技 → 持有方回 200（500→700）。
//   C 3144 [0,0,1,150]：攻击技 → 持有方回 150（500→650）。
//   D 3219 [1,60]：攻击技 → 木桩多掉 已损×60%。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;
constexpr int kGaoSu = 20017;   // 高速移动：属性技（无伤害噪声）

SoulMark make_mark(int effect_id, std::vector<int> db_args, int monster_id) {
    SoulMark sm(effect_id, "K" + std::to_string(effect_id), "test",
                EffectArgs(std::move(db_args)));
    sm.monster_id = monster_id;
    return sm;
}

// 持有方（0 方）以 skill_id 出手；holder_hp 可压低（触发"已损"）。
struct Probe {
    int holder_hp_end = 0;
    int dummy_loss = 0;
};

Probe probe_use_skill(std::vector<int> db_args, int monster_id, int skill_id, int holder_hp) {
    ScenarioBattle b(scenario::make_party({scenario::make_pet(
        1, "持有方", {skill_id, 10005, 10005, 10005, 10005},
        /*speed=*/300, /*atk=*/5000, /*hp=*/1000000,
        make_mark(693, std::move(db_args), monster_id))}),
        scenario::make_party(scenario::make_pet(2, "木桩", {10005, 10005, 10005, 10005, 10005},
                                                50, 10, 1000000)));
    BattleContext& ctx = b.ctx();
    ctx.seed_rng(20261004);
    b.start();
    ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = holder_hp;
    const int dummy_hp0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return {ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp,
            dummy_hp0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // A 攻击技附已损 50%（对照：同 rand 序无粉伤版）
    {
        const int red_only = probe_use_skill({0, 0, 0, 0}, 2992, kPaiDa, 500).dummy_loss;
        CHECK(red_only > 1000, "红伤基线量级合理");
        const Probe p = probe_use_skill({1, 50, 2, 200}, 2992, kPaiDa, 500);
        CHECK_EQ(p.dummy_loss, red_only + 999500 * 50 / 100,
                 "A 攻击技附加 已损(999500)×50% 粉伤");
    }
    // B 属性技回 200
    {
        const Probe p = probe_use_skill({1, 50, 2, 200}, 2992, kGaoSu, 500);
        CHECK_EQ(p.holder_hp_end, 700, "B 属性技 → 恢复 200（500→700）");
        CHECK_EQ(p.dummy_loss, 0, "B2 属性技无伤害");
    }
    // C 攻击技回 150
    {
        const Probe p = probe_use_skill({0, 0, 1, 150}, 3144, kPaiDa, 500);
        CHECK_EQ(p.holder_hp_end, 650, "C 攻击技 → 恢复 150（500→650）");
    }
    // D 3219 半行 [1,60]
    {
        const int red_only = probe_use_skill({0, 0, 0, 0}, 3219, kPaiDa, 500).dummy_loss;
        const Probe p = probe_use_skill({1, 60}, 3219, kPaiDa, 500);
        CHECK_EQ(p.dummy_loss, red_only + 999500 * 60 / 100, "D 攻击技附加 已损×60% 粉伤");
    }

    std::cout << "scenario_2871 PASS (K10 effect693 双节点程序)" << std::endl;
    return 0;
}
