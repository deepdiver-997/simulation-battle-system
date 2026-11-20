// 场景 5390 —— J212 对手主动切换异常窗族（2040，moves_lib/j_opp_switch_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5390-5399）
//
// 断言口径：
//   - 2040（斗争宣言 29335 链 [191,1838,2040]|[4,2,2,100,2]）：
//     R1 施放 → 注册 {0}=2 回合 EVENT_SWAP 窗；R2 对手主动中切 →
//     登场的新宠获得 {2}=烧伤(2)（100% 档，无随机分支）。
//     R3 对手再次主动中切 → 窗已过期（register_round=R1，duration=2 → R1/R2 有效），
//     新登场者不得烧伤（精确异常位断言）。
//   - 对照：R1 不施放 → R2 同样中切 → 对手无烧伤。
//   - 191 兄弟=自身 4 回合免疫反弹异常，作用于施放侧，不影响对手侧异常；1838 未实现=哑。
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

constexpr int kPaiDa    = 10004;  // 拍打：物 40（填充刀）
constexpr int kDouZheng = 29335;  // [191,1838,2040]|[4,2,2,100,2]（2 回合/100%/烧伤）
constexpr int kBurn     = static_cast<int>(AbnormalStatusId::Burn);  // 烧伤 2
constexpr int kMaxHp    = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_caster() {
    ElfPet p = make_pet(1, "己方A", {kDouZheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_bench() {
    ElfPet p = make_pet(1, "己方B", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/200, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

std::array<ElfPet, 6> party0() {
    ElfPet a = mk_caster();
    ElfPet b = mk_bench();
    return {a, b, b, b, b, b};
}

std::array<ElfPet, 6> party1() {
    ElfPet a = mk_foe();
    return {a, a, a, a, a, a};
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5390 2040 对手主动中切 → 登场者烧伤；窗过期后不再触发 ══
    {
        {   // (a) R1 施放 → R2 对手中切 → 新宠烧伤；R3 再切 → 过期不触发
            ScenarioBattle b(party0(), party1());
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 1, "T5390.a 前置：对手 slot1 已登场");
            CHECK(ctx.has_active_abnormal_status(1, kBurn),
                  "T5390.a 2040：窗内对手主动中切 → 登场者烧伤(2)");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 0, "T5390.a 前置：对手 slot0 再登场");
            CHECK(!ctx.has_active_abnormal_status(1, kBurn),
                  "T5390.a 2040：窗过期（R3 ≥ R1+2）→ 再切不烧伤");
        }
        {   // (b) 对照：R1 不施放 → R2 同样中切 → 无烧伤
            ScenarioBattle b(party0(), party1());
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 1, "T5390.b 前置：对手 slot1 已登场");
            CHECK(!ctx.has_active_abnormal_status(1, kBurn),
                  "T5390.b 2040 对照：未施放 → 中切不给异常");
        }
        std::cout << "  [T5390] 2040 对手主动切换异常窗 + 窗过期 + 对照 通过\n";
    }

    std::cout << "scenario_5390 PASS (J212 对手主动切换异常窗族)" << std::endl;
    return 0;
}
