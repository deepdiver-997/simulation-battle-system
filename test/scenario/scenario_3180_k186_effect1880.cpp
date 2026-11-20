#include <utils/sbs_rand.h>
// 场景 3180 —— K186 effect 1880（已损体力动态概率麻痹+追击族）：吕蒙 [10,1]/[20,1]
//
//   A 追击（row1 10%）：对手已损 1000 → 攻击落地后总掉血 = 红伤 2 + 粉伤 100（精确）。
//   B 追击（row2 20%）：同布局 → 红伤 2 + 粉伤 200。
//   C 对手满血 → 追击 0（已损 0，子句静默）。
//   D 麻痹确定性关：自身满血（已损 0% → roll 短路）→ 40 回合对手零麻痹。
//   E 麻痹近确定性开：自身已损 99% → 40 回合对手麻痹回合数 ≥30。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60 必中（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);

SoulMark make_mark(int db0) {
    SoulMark sm(1880, "K1880", "test", EffectArgs({db0, 1}));
    sm.monster_id = 4295;
    return sm;
}

// 0 方带 1880 攻击一回合（1 方 NONE），返回对手总掉血。atk=1 → 红伤保底 2 确定性
//（K169 T3030 同款精确差账；atk 大时 85%~100% 浮动毁掉等式断言）。
int probe_pursue(int db0, int foe_cur_hp) {
    ElfPet holder = scenario::make_pet(1, "吕蒙", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/1, /*hp=*/1000000,
                                       make_mark(db0));
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = foe_cur_hp;
    const int foe0 = ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
    return foe0 - ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 追击 10%：对手已损 1000 → 2 + 100 ═══════════════════════════════
    CHECK_EQ(probe_pursue(10, 1000000 - 1000), 102, "A row1：追击 = 已损1000×10% = 100");

    // ══ B 追击 20%：对手已损 1000 → 2 + 200 ═══════════════════════════════
    CHECK_EQ(probe_pursue(20, 1000000 - 1000), 202, "B row2：追击 = 已损1000×20% = 200");

    // ══ C 对手满血 → 追击 0（只剩红伤）════════════════════════════════════
    {
        const int loss = probe_pursue(10, 1000000);
        CHECK(loss > 0 && loss < 10, "C 对手满血：无追击（只剩红伤保底）");
    }

    // ══ D 麻痹确定性关：自身满血 40 回合零麻痹 ════════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "吕蒙", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(10));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
        }
        CHECK_EQ(ctx.has_active_abnormal_status(1, kParalysis), false,
                 "D 满血 0%：40 回合对手零麻痹");
    }

    // ══ E 麻痹近确定性开：自身已损 99% → ≥30/40 ═══════════════════════════
    {
        ElfPet holder = scenario::make_pet(1, "吕蒙", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                           /*speed=*/300, /*atk=*/10, /*hp=*/1000000,
                                           make_mark(10));
        ScenarioBattle b(scenario::make_party(holder),
                         scenario::make_party(scenario::make_pet(
                             2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                             /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
        BattleContext& ctx = b.ctx();
        sbs_srand(20261007);
        b.start();
        ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp = 10000;   // 已损 99%
        int wins = 0;
        for (int r = 0; r < 40; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            // 每回合独立观测：查完即清（dispel_active_anomalys 同族原语）——
            // 麻痹持续 1~3 回合会把"新增"计数塌成 1（实测首版翻车点）。
            if (ctx.has_active_abnormal_status(1, kParalysis)) {
                ++wins;
                (void)dispel_active_anomalies(&ctx, 1);
            }
        }
        std::cout << "  [E] 40 回合麻痹落地 " << wins << "（期望 ~39）" << std::endl;
        CHECK(wins >= 30, "E 已损 99%：麻痹落地 ≥30/40");
    }

    std::cout << "scenario_3180 PASS (K186 1880 已损麻痹+追击)" << std::endl;
    return 0;
}
