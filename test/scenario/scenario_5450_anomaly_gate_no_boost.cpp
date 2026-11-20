// 场景 5450 —— J215 无提升门异常族（2004，moves_lib/j_anomaly_gate_no_boost.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5450-5459）
//
// 断言口径：
//   - 2004（光刃痛击 36886 纯链 [2004]|[100,20]）：{0}=100%、{1}=失明(20)。
//   - a) 对手无能力提升 → 门开 → 对手失明。
//   - b) 对手预置攻+1 → 门闭 → 对手不失明（同一招、同一随机面；精确异常位断言）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物 40（填充刀）
constexpr int kGuangRen = 36886; // [2004]|[100,20]（纯链；100% 失明）
constexpr int kBlind  = static_cast<int>(AbnormalStatusId::Blind);  // 失明 20
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kGuangRen, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5450 2004 对手无提升门 → 100% 失明；有提升 → 门闭 ══
    {
        {   // (a) 对手无提升 → 失明
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kBlind),
                  "T5450.a 2004：对手无提升 → 100% 失明(20)");
        }
        {   // (b) 对手攻+1 → 门闭 → 不失明
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 0, 1);   // 对手攻击 +1
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], 1, "T5450.b 前置：对手攻+1 仍在");
            CHECK(!ctx.has_active_abnormal_status(1, kBlind),
                  "T5450.b 2004：对手有提升 → 门闭不失明");
        }
        std::cout << "  [T5450] 2004 无提升门失明 + 有提升门闭 通过\n";
    }

    std::cout << "scenario_5450 PASS (J215 无提升门异常族)" << std::endl;
    return 0;
}
