#include <utils/sbs_rand.h>
// 场景 3480 —— K207 effect 301（对手异常威力翻倍族）：绝影·天煞 2598（零参）
//
// 威力改写重掷浮动 → 比例断言不可对齐（速查 §五.1/K13 勘误）——用大攻差账做
// 方向性断言 + 同种子种植对齐对照：
//   A 对手有异常：伤害 > 同种子无异常对照（翻倍必大于基线，攻 5000 量级不受地板抹平）。
//   B 对照自等：两次同种子无异常对局伤害逐位相等（随机对齐有效性自证）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kBurn = 2;

// opp_abnormal=对手种烧伤；cure=种植后清除（rand 消耗对齐用）。返回对手掉血。
int probe_loss(bool opp_abnormal, bool cure) {
    ElfPet holder = scenario::make_pet(1, "绝影·天煞", {kYanFan, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                       /*speed=*/300, /*atk=*/5000, /*hp=*/1000000);
    ElfPet foe = scenario::make_pet(2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                    /*speed=*/100, /*atk=*/10, /*hp=*/1000000);
    scenario::set_crit_sixteenths(holder, 0);   // 排除暴击干扰
    ScenarioBattle b(scenario::make_party(holder), scenario::make_party(foe));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    if (opp_abnormal) {
        (void)apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0, /*chance_pct=*/100,
                            ChanceSource::Skill);
        if (cure) {
            (void)dispel_active_anomalies(&ctx, 1);
        }
    }
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

    // ══ A 翻倍 > 基线（方向性，重掷浮动不可对齐）══════════════════════════
    const int doubled = probe_loss(/*opp_abnormal=*/true, /*cure=*/false);
    const int baseline = probe_loss(false, false);
    CHECK(doubled > baseline, "A 对手异常：威力翻倍伤害大于基线");
    std::cout << "  [A] doubled=" << doubled << " baseline=" << baseline << std::endl;

    // ══ B 同种子种植对齐自证：两局无异常伤害逐位相等 ══════════════════════
    {
        const int c1 = probe_loss(false, false);
        const int c2 = probe_loss(true, /*cure=*/true);   // 种后清=同 rand 消耗、无异常
        CHECK_EQ(c1, c2, "B 同种子对齐自证（无异常两局等伤）");
    }

    std::cout << "scenario_3480 PASS (K207 301 对手异常威力翻倍)" << std::endl;
    return 0;
}
