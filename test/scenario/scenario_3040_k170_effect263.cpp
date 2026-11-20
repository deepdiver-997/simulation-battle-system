#include <utils/sbs_rand.h>
// 场景 3040 —— K170 effect 263（开场连击降防族）：拉姆/拉姆塞
// "开场两回合内所有直接攻击都会使对手防御-1"（零参，两行同构）
//
// 断言口径（等级槽 2=防御，本体 ability_levels）：
//   A 窗内首击：第 1 回合直接攻击 → 对手防御 -1。
//   B 每击结算：第 1、2 回合各一击 → 累计 -2（窗内每击一跳）。
//   C 窗外：第 3 回合才出手 → 防御不动（roundCount+1 > 2 门）。
//   D 属性技排除：窗内用属性技 → 防御不动（used_attribute 门）。
//   E 主语=持有方：对手出手 → 双方防御都不动。
//   F 第二行同构：拉姆塞 2453 同行为。
#include <cstdlib>
#include <iostream>
#include <initializer_list>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技（used_attribute=true）
constexpr int kDefSlot = 2;      // 等级槽序 0攻 1特攻 2防 3特防 4速 5命中

SoulMark make_mark(int monster_id) {
    SoulMark sm(263, "K263", "test", EffectArgs());
    sm.monster_id = monster_id;
    return sm;
}

// 0 方带魂印 263 跑 rounds 回合（attack_rounds 里的回合用 skill 出手，其余 NONE），
// 返回 1 方（对手）防御等级。
int probe_opp_def(int monster_id, int rounds, std::initializer_list<int> attack_rounds,
                  int skill_id) {
    ElfPet holder = scenario::make_pet(1, "拉姆", {skill_id, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/100000,
                                       make_mark(monster_id));
    ElfPet foe = scenario::make_pet(2, "沙包", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                    /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
    ScenarioBattle b(scenario::make_party(holder), scenario::make_party(foe));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    for (int r = 1; r <= rounds; ++r) {
        bool attacks = false;
        for (int ar : attack_rounds) {
            if (ar == r) {
                attacks = true;
            }
        }
        b.submit_operation(0, attacks ? BattleFsm::ActionType::SELECT_SKILL
                                      : BattleFsm::ActionType::NONE,
                           attacks ? 0 : -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
    }
    return ctx.ability_levels[1][kDefSlot];
}

// E 探针：对手出手（0 方 NONE），返回双方防御等级 {自, 对}。
std::pair<int, int> probe_foe_attacks() {
    ElfPet holder = scenario::make_pet(1, "拉姆", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/100, /*atk=*/10, /*hp=*/1000000,
                                       make_mark(2452));
    ElfPet foe = scenario::make_pet(2, "对手", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                    /*speed=*/300, /*atk=*/10, /*hp=*/1000000);
    ScenarioBattle b(scenario::make_party(holder), scenario::make_party(foe));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    return {ctx.ability_levels[0][kDefSlot], ctx.ability_levels[1][kDefSlot]};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 窗内首击（拉姆 2452）════════════════════════════════════════════
    CHECK_EQ(probe_opp_def(2452, 1, {1}, kYanFan), -1, "A 第 1 回合攻击 → 对手防御 -1");

    // ══ B 每击结算：两回合各一击 → -2 ═════════════════════════════════════
    CHECK_EQ(probe_opp_def(2452, 2, {1, 2}, kYanFan), -2, "B 第 1、2 回合各一击 → 累计 -2");

    // ══ C 窗外：第 3 回合出手 → 不动 ══════════════════════════════════════
    CHECK_EQ(probe_opp_def(2452, 3, {3}, kYanFan), 0, "C 第 3 回合攻击 → 窗已过不动");

    // ══ D 属性技排除：窗内属性技 → 不动 ═══════════════════════════════════
    CHECK_EQ(probe_opp_def(2452, 1, {1}, kGaoSu), 0, "D 窗内属性技 → 非直接攻击不动");

    // ══ E 主语=持有方：对手出手 → 双方防御不动 ════════════════════════════
    {
        const auto [own, opp] = probe_foe_attacks();
        CHECK_EQ(own, 0, "E1 对手攻击 → 持有方防御不动");
        CHECK_EQ(opp, 0, "E2 对手攻击 → 对手自己防御也不动");
    }

    // ══ F 第二行同构（拉姆塞 2453）════════════════════════════════════════
    CHECK_EQ(probe_opp_def(2453, 1, {1}, kYanFan), -1, "F 拉姆塞同文同行为 → -1");

    std::cout << "scenario_3040 PASS (K170 263 开场连击降防：2回合窗+每击结算)" << std::endl;
    return 0;
}
