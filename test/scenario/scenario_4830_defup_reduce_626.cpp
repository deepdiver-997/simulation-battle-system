// 场景 4830 —— K327 双防提升+减伤族（626，soul_lib/k327_defup_reduce.cpp）
//（2026-10-21，worktree ../agents/k327；号段 4830-4839 → T4830-T4832 三块）
//
// 断言口径（626「自身的双防能力值提升30%;受到的伤害减少20%」）：
//   - T4830 **面板双防提升**：进场后印主的 numericalBase[DEFENSE] 与
//     [SPECIAL_DEFENSE] 各提升 30%（100 → 130）。⚠️ 口径：new = old + old×30%
//     （"提升到 130%"）——全库仅本行"双防能力值提升X%"，无同族锚定，基数记档。
//   - T4831 **减伤 20%**：ROUND_START 后 ws.damage_reduce_add[0] 含 20
//     （加算减伤百分比，REDUCE_PCT 阶段消费）。
//   - T4832 **每回合重写减伤槽**：连续两回合 → 第二回合减伤槽仍为 20
//     （ws 每回合 reset → 不重写就只减一回合）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {30,20}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 起的夜跑口径，见实现头注「行参读取路径」）。
// ⚠️ 面板数值读 **ctx.getPet(0).numericalBase[...]**（k249 2522 面板直加的落点）；
//   减伤槽读 **ctx.ws.damage_reduce_add[0][i]**（4 槽并列加算项）。
// ⚠️ make_pet 默认 DEFENSE=100 / SPECIAL_DEFENSE=100（速查 §五）→ 提升后各 130。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;    // 撞击：filler
constexpr int kMaxHp = 99999;
constexpr int kBaseDef = 100;  // make_pet 默认防御/特防

SoulMark make_mark() {
    // DB args 原样 [30,20]：db[0]=30（"提升30%"）、db[1]=20（"减少20%"）——双参自证。
    SoulMark sm(626, "K327 魂印", "test", EffectArgs(std::vector<int>{30, 20}));
    sm.monster_id = 2881;
    return sm;
}

ElfPet mark_pet(bool with_mark = true) {
    ElfPet p = with_mark
        ? make_pet(1, "马尔修斯（坚壁型）", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/10, kMaxHp, make_mark())
        : make_pet(1, "裸马尔修斯", {kJi, kJi, kJi, kJi, kJi},
                   /*speed=*/100, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/10, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 减伤槽合计（4 槽并列加算项）。
int reduce_sum(const BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < 4; ++i) {
        sum += ctx.ws.damage_reduce_add[side][i];
    }
    return sum;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4830 面板双防提升 30% ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int def = ctx.getPet(0).numericalBase[NumericalPropertyIndex::DEFENSE];
        const int spdef = ctx.getPet(0).numericalBase[NumericalPropertyIndex::SPECIAL_DEFENSE];
        CHECK_EQ(def, kBaseDef + kBaseDef * 30 / 100,
                 "T4830 626：自身防御面板 100 → 130（+30%），实测 " + std::to_string(def));
        CHECK_EQ(spdef, kBaseDef + kBaseDef * 30 / 100,
                 "T4830 626：自身特防面板 100 → 130（+30%），实测 " + std::to_string(spdef));
        std::cout << "  [T4830] 面板双防提升 通过\n";
    }

    // ══ T4831 减伤 20% 写入 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(reduce_sum(ctx, 0), 20,
                 "T4831 626：受到的伤害减少 20% → 加算减伤槽 = 20（实测 "
                     + std::to_string(reduce_sum(ctx, 0)) + "）");
        std::cout << "  [T4831] 减伤写入 通过\n";
    }

    // ══ T4832 每回合重写减伤槽（不残留、不失效）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int r1 = reduce_sum(ctx, 0);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int r2 = reduce_sum(ctx, 0);
        CHECK_EQ(r1, 20, "T4832 626：第 1 回合减伤槽 = 20（实测 " + std::to_string(r1) + "）");
        CHECK_EQ(r2, 20,
                 "T4832 626：第 2 回合减伤槽仍 = 20（ws 每回合 reset → **必须每回合重写**），"
                 "实测 " + std::to_string(r2));
        std::cout << "  [T4832] 每回合重写减伤槽 通过\n";
    }

    std::cout << "scenario_4830 PASS (K327 双防提升+减伤族)" << std::endl;
    return 0;
}
