#include <utils/sbs_rand.h>
// 场景 3570 —— K214 effect 318（先手概率烧伤族）：克洛伊 2646（args 不消费）
//
//   A 先手 40 攻：烧伤落地带 2~14（binomial(40,0.2) 均值 8，查后 dispel 独立观测）。
//   B 后手 40 攻：零烧伤（先手门确定性关）。
//   C 先手属性技 20 回合：零烧伤（used_attribute 门）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

SoulMark make_mark() {
    SoulMark sm(318, "K318", "test", EffectArgs({1, 20, 2}));
    sm.monster_id = 2646;
    return sm;
}

// 0 方带 mark 攻 rounds 回合（1 方也攻击但载体属性技不干扰），返回对手烧伤落地数。
int probe_burns(int my_speed, bool attribute_only, int rounds) {
    ElfPet holder = scenario::make_pet(
        1, "克洛伊",
        attribute_only ? std::array<int, 5>{kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu}
                       : std::array<int, 5>{kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
        /*speed=*/my_speed, /*atk=*/5000, /*hp=*/1000000, make_mark());
    ElfPet foe = scenario::make_pet(2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                                    /*speed=*/200, /*atk=*/10, /*hp=*/1000000);
    ScenarioBattle b(scenario::make_party(holder), scenario::make_party(foe));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    int wins = 0;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        if (ctx.has_active_abnormal_status(1, kBurn)) {
            ++wins;
            (void)dispel_active_anomalies(&ctx, 1);
        }
    }
    return wins;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 先手（速 300>200）40 攻：落地带 2~14 ════════════════════════════
    {
        const int wins = probe_burns(300, /*attribute_only=*/false, 40);
        std::cout << "  [A] 40 回合烧伤落地 " << wins << "（期望 ~8）" << std::endl;
        CHECK(wins >= 2 && wins <= 14, "A 20% 档 40 回合统计带 2~14");
    }

    // ══ B 后手（速 100<200）40 攻：零烧伤 ═════════════════════════════════
    CHECK_EQ(probe_burns(100, false, 40), 0, "B 后手：零烧伤（先手门关）");

    // ══ C 先手属性技 20 回合：零烧伤 ══════════════════════════════════════
    CHECK_EQ(probe_burns(300, /*attribute_only=*/true, 20), 0, "C 属性技：零烧伤");

    std::cout << "scenario_3570 PASS (K214 318 先手概率烧伤)" << std::endl;
    return 0;
}
