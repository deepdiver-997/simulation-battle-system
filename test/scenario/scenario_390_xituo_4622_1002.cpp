#include <utils/sbs_rand.h>
// 场景 390：希拓 4622 第五技「神煌炎舞斩 37222」缺失效果 1002
//（moves_lib/d5_D23.cpp，夜跑 night-2，2026-09-28）
//
// 1002：对手处于异常状态 → 先制+1。
// 断言组：
//   TA 主路径：对手带烧伤（直写槽）→ R2 使用时先制+1（慢者先动）。
//   TB 对照：对手无异常 → 无先制（慢者后手）。
//   首用偏差记档：每场首用无武装点 → 不加先制（与 1098/1782 同根源），R1 双侧均不断言。
//
// 确定性：37222/拍打 crit_rate=0、必中；烧伤直写槽不触发回合扣减随机；
// 链内 942/1960 对无强化白板对手无行为。
#include <array>
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kShenHuang = 37222;  // 神煌炎舞斩：火神秘物攻 160、必中、effect [1002,942,1960]
constexpr int kPaDa = 10004;       // 拍打：物攻 40、无效果

ElfPet xituo(int speed) {
    ElfPet p = scenario::make_pet(4622, "希拓", {kShenHuang, kPaDa, kPaDa, kPaDa, kPaDa},
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

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 主路径：对手异常 → 先制+1（慢者先动）==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xituo(/*speed=*/50)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手带烧伤（异常状态；直写槽，覆盖本回合与下一回合判定）
        ctx.abnormal_status_end_round[1][static_cast<std::size_t>(AbnormalStatusId::Burn)] =
            ctx.roundCount + 9;
        drive_round(b, 0, 0);   // R1：首用（武装常驻节点；本用不加先制——偏差记档）
        drive_round(b, 0, 0);   // R2：对手仍处异常 → 先制+1
        CHECK(ctx.has_any_active_abnormal_status(1), "TA 前提：对手处于异常状态");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "TA 核心：先制 +1 已置位");
        CHECK(!ctx.is_second_mover(0), "TA 核心：慢者因先制+1 先动");
        std::cout << "  [TA] 对手异常先制+1 通过\n";
    }

    //========== TB 对照：对手无异常 → 无先制 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(xituo(/*speed=*/50)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        drive_round(b, 0, 0);
        CHECK(!ctx.has_any_active_abnormal_status(1), "TB 前提：对手无异常");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "TB 核心：无先制加成");
        CHECK(ctx.is_second_mover(0), "TB 核心：慢者后手");
        std::cout << "  [TB] 无异常对照 通过\n";
    }

    std::cout << "scenario_390 all passed" << std::endl;
    return 0;
}
