// 场景 860 —— H8 恢复族缺口 509（moves_lib/h_lost_hp_heal.cpp）
//（2026-09-30，worktree ../agents/h8；号段 860-869；工单四 id 中 532/597/707 已有
//  实现（round_window.cpp:496-498，场景 038 等既有覆盖），本场景只补真缺口 509）
//
// 断言口径：恢复量 = 已损失体力 ×1/{0}（整型除法向下取整）→ hp 终值精确等式；
// 满血对照（已损失 0 → last_heal==0）；封回血（HEAL_BLOCK 票，017/H1 同款）→ 恢复 0。
// 载体 accuracy=100 消 miss 噪声（不置 must_hit——H5 记档）；载体红伤打对手侧无断言。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYeYu   = 15326;  // 叶语风花闪：[509] → [2]
constexpr int kZhaoZe = 16789;  // 沼泽狂风斩：[509] → [3]
constexpr int kMieShi = 16877;  // 灭世斩杀：[509] → [2]
constexpr int kPaDa   = 10004;
constexpr int kMaxHp  = 99999;

void accuracy_100(ElfPet& p, bool force_must_hit) {
    for (Skills& s : p.skills) {
        if (force_must_hit) {
            s.must_hit = 1;
        }
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*force_must_hit=*/true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*force_must_hit=*/false);
    return p;
}

void play(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // T1 叶语风花闪 [2]：损失 99399 → 恢复 99399/2=49699 → hp 600+49699=50299
    {
        ScenarioBattle b(make_party(self_pet({kYeYu, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 600;
        play(b, 0);
        CHECK_EQ(ctx.getPet(0).hp, 600 + 99399 / 2, "T1 核心：恢复已损失 1/2（整除）");
        CHECK_EQ(ctx.ws.last_heal_amount[0], 99399 / 2, "T1 核心：last_heal 精确");
        std::cout << "  [T1] 509 [2] 精确恢复通过\n";
    }

    // T2 沼泽狂风斩 [3]：损失 98999 → 恢复 98999/3=32999 → hp 1000+32999=33999
    {
        ScenarioBattle b(make_party(self_pet({kZhaoZe, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        play(b, 0);
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 98999 / 3, "T2 核心：第二载体 [3] 分母");
        std::cout << "  [T2] 509 [3] 分母通过\n";
    }

    // T3 满血对照：已损失 0 → 无恢复
    {
        ScenarioBattle b(make_party(self_pet({kYeYu, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        play(b, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T3 核心：满血已损失 0 → 恢复 0");
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T3 核心：hp 不变");
        std::cout << "  [T3] 509 满血对照通过\n";
    }

    // T4 封回血：HEAL_BLOCK → 消封、恢复 0
    {
        ScenarioBattle b(make_party(self_pet({kYeYu, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 600;
        ctx.grant_immunity(0, ImmunityType::HEAL_BLOCK, coverage_all(), /*anomaly_mask=*/0,
                           /*duration_rounds=*/0, /*source_id=*/8001);
        play(b, 0);
        CHECK_EQ(ctx.ws.last_heal_amount[0], 0, "T4 核心：封回血 → 恢复 0（统一出口）");
        CHECK_EQ(ctx.getPet(0).hp, 600, "T4 核心：hp 不变");
        std::cout << "  [T4] 509 封回血通过\n";
    }

    // T5 第三载体灭世斩杀 [2]：损失 90000（hp=9999）→ 恢复 45000
    {
        ScenarioBattle b(make_party(self_pet({kMieShi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 9999;
        play(b, 0);
        CHECK_EQ(ctx.getPet(0).hp, 9999 + 90000 / 2, "T5 核心：第三载体同模板");
        std::cout << "  [T5] 509 第三载体通过\n";
    }

    std::cout << "scenario_860_lost_hp_heal all passed" << std::endl;
    return 0;
}
