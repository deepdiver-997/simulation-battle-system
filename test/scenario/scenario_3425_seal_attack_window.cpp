#include <utils/sbs_rand.h>
// 场景 3425 —— J183 封攻击技窗族（1923，moves_lib/j_seal_attack_window.cpp）
//（2026-10-07，worktree ../agents/j183，看板段 3425-3434 → T3425-T3426）
//
// 载体：28283 长相思（1923=[1] 封对手攻击技 1 回合；链上 191 反弹异常窗无异常可反、
// 1396 每回合吸取窗会磨对手血（场景每回合回满对手防阵亡）、478 封属性技窗不涉攻击技）。
//
// 断言口径：
//   - T3425 窗内/窗外：R1 持印者先手出手 → 对手当回合攻击被无效（持印者 0 失血）；
//     R2 改用拍打（不续封）→ 窗过期 → 对手攻击落地（失血 > 0）。
//     ⚠️ 实测记档：每回合重复使用会每次重新授予 1 回合窗（逐次使用逐次生效），
//     同技连用时对手攻击持续被无效——这是正确行为，测试须换技才能观察到过期。
//   - T3426 对照（无印）：R1 对手攻击照常落地（失血 > 0）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier = 28283;   // 长相思
constexpr int kMaxHp = 99999;

ElfPet holder_pet(bool with_mark, int speed) {
    ElfPet p = with_mark
        ? make_pet(1, "印主", {kCarrier, 10004, 10004, 10004, 10004}, speed,
                   /*atk=*/100, /*hp=*/kMaxHp)
        : make_pet(1, "对照", {10004, 10004, 10004, 10004, 10004}, speed,
                   /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/50, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
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

    // ══ T3425 窗内无效 / 窗外恢复 ══
    {
        ScenarioBattle b(make_party(holder_pet(/*with_mark=*/true, /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp, "T3425 1923：窗内对手攻击被无效（0 失血）");
        // 链上 1396 吸取窗每回合磨对手 1/3 最大体力 → 回满对手防其 R2 阵亡
        //（跳过 second mover 流程会使窗外断言空洞）。
        ctx.getPet(1).hp = kMaxHp;
        const int hp_after_r1 = ctx.getPet(0).hp;
        std::cout << "  [probe] R2-start hp0=" << ctx.getPet(0).hp
                  << " hp1=" << ctx.getPet(1).hp << "\n";
        // R2 换拍打（不再续封）→ 窗过期、对手攻击落地
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(ctx.getPet(0).hp < hp_after_r1, "T3425 1923：不续封 → 窗过期对手攻击落地");
        std::cout << "  [T3425] 封攻击技窗 1 回合 通过\n";
    }

    // ══ T3426 对照：无印 R1 照常承伤 ══
    {
        ScenarioBattle b(make_party(holder_pet(/*with_mark=*/false, /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().getPet(0).hp < kMaxHp, "T3426 对照：无印 → 对手攻击落地");
        std::cout << "  [T3426] 对照 通过\n";
    }

    std::cout << "scenario_3425 PASS (J183 封攻击技窗族)" << std::endl;
    return 0;
}
