#include <utils/sbs_rand.h>
// 场景 250：不灭·艾恩斯 3467 第五技「王·焚世烈焰 31273」缺失效果 777
//（moves_lib/d5_D9.cpp，夜跑 night-2，2026-09-28）
//
// 777：消除对手能力上升状态，消除成功下 1 回合（切片 [1]）必定先出手。
// 断言通道：ws.guaranteed_first（必先等级，先手权结算开头 memset、窗口节点重写）+
// ctx.is_second_mover（先手权结果派生）。艾恩斯速度 50 慢于对手 100：无必先时对手
// 先手；消除成功后的下一回合艾恩斯必先。
//
// 断言组：
//   T1 主路径：对手带 2 槽强化 → R1 消强成功 → R2 艾恩斯必先（慢者先动）、
//      guaranteed_first=1；R3 窗口（1 回合）已过 → 对手恢复先手、必先清零。
//   T2 消除失败对照：对手无强化 → 消强失败 → R2 无必先、对手照常先手。
//
// 确定性：31273/拍打 crit_rate=0、必中；链内 1048（对手无异常→不触发）、1257（吸体，
// 只影响自身体力）、760（同系不微弱）均不干扰必先断言。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kFenShi = 31273;  // 王·焚世烈焰：火系物攻 160、必中、effect [760,777,1048,1257]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果

ElfPet aiensi(int speed) {
    ElfPet p = scenario::make_pet(0, "不灭·艾恩斯", {kFenShi, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
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

    //========== T1 主路径：消强成功 → 下 1 回合必先 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(aiensi(/*speed=*/50)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手带 2 槽正等级（消强有目标）
        ctx.ability_levels[1][0] = 2;
        ctx.ability_levels[1][1] = 2;
        // R1：对手先手拍打，艾恩斯后手焚世烈焰 → 消强成功、武装"下1回合必先"。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.ability_levels[1][0] == 0 && ctx.ability_levels[1][1] == 0,
              "T1 前提：对手强化已被消除");
        CHECK(ctx.is_second_mover(0), "T1 前提：R1 艾恩斯后手（速度慢）");
        // R2：慢者必先——艾恩斯先动。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ws.guaranteed_first[0], 1, "T2 核心：必先等级=1 已置位");
        CHECK(!ctx.is_second_mover(0), "T1 核心：R2 艾恩斯（慢者）必先出手");
        CHECK(ctx.is_second_mover(1), "T1 核心：R2 对手被压到后手");
        // R3：窗口 1 回合已过 → 对手恢复先手。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ws.guaranteed_first[0], 0, "T1：R3 必先已清零（窗口 1 回合）");
        CHECK(ctx.is_second_mover(0), "T1 核心：R3 艾恩斯恢复后手");
        std::cout << "  [T1] 消强成功 → 下1回合必先 → 过期 通过\n";
    }

    //========== T2 消除失败对照：对手无强化 → 无必先 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(aiensi(/*speed=*/50)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.is_second_mover(0), "T2 前提：R1 艾恩斯后手");
        // R2：消除失败（对手本无强化）→ 无必先，对手照常先手。
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.drive();
        CHECK_EQ(ctx.ws.guaranteed_first[0], 0, "T2：无必先置位");
        CHECK(ctx.is_second_mover(0), "T2 核心：消除失败 → 艾恩斯仍后手");
        std::cout << "  [T2] 消除失败对照 通过\n";
    }

    std::cout << "scenario_250 all passed" << std::endl;
    return 0;
}
