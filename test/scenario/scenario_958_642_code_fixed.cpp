#include <utils/sbs_rand.h>
// 场景 958 —— K4 槽码表 stat 对 + 每回合定额粉伤（effect 642）
//
// 缪斯行 args "2 1 4 1 150 1"：(code,delta)×2 = 特攻+1 速+1（code 表 {2:特攻,4:速}，
// 锚定两行 tips）+ fixed=150；玄冰行 "2 0 4 0 150"：+0/+0（无 stat 变化）+ 150。
// "每回合附加150点固定伤害" = ROUND_START 粉伤 FIXED 给对手。
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

// 0 方带魂印，双方 NONE 一回合，返回 {对手损血, 0 方等级数组拷贝}。
struct RoundResult {
    int foe_loss = 0;
    std::array<int, 6> levels{};
};

RoundResult probe_round(const SoulMark& mark) {
    ScenarioBattle b(scenario::make_party(scenario::make_pet(
                         1, "持印者", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000, mark)),
                     scenario::make_party(scenario::make_pet(
                         2, "对手", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20260916);
    b.start();
    const int hp1 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    RoundResult r;
    r.foe_loss = hp1 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    for (int i = 0; i < 6; ++i) {
        r.levels[i] = ctx.ability_levels[0][i];
    }
    return r;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 玄冰行：无 stat 变化 + 对手每回合 -150 ════════════════════════════
    {
        const RoundResult r = probe_round(make_mark(642, {2, 0, 4, 0, 150}, 2969));
        CHECK_EQ(r.foe_loss, 150, "A 玄冰：对手回合开始吃 150 固定伤害");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(r.levels[i], 0, "A 玄冰：无能力变化（delta=0）");
        }
    }
    // ══ B 缪斯行：特攻+1 速+1 + 150 ════════════════════════════════════════
    {
        const RoundResult r = probe_round(make_mark(642, {2, 1, 4, 1, 150, 1}, 2852));
        CHECK_EQ(r.foe_loss, 150, "B 缪斯：150 固定伤害");
        CHECK_EQ(r.levels[1], 1, "B 缪斯：特攻 +1（code 2）");
        CHECK_EQ(r.levels[4], 1, "B 缪斯：速度 +1（code 4）");
        CHECK_EQ(r.levels[0], 0, "B 缪斯：攻击不动（code 表校验）");
    }

    std::cout << "scenario_958 PASS (K4 642 槽码表 + 每回合定额粉伤)" << std::endl;
    return 0;
}
