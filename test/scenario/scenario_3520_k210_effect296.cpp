#include <utils/sbs_rand.h>
// 场景 3520 —— K210 effect 296（低血攻附麻痹族）：净天云神 2585（args[1,50,6] 不消费）
//
//   A 门开（对手 20% 血）+ 攻击 40 回合：麻痹落地 ≤6（6% 稀疏档宽带，
//     binomial(40,0.06) 均值 2.4——抓"恒触发/恒不触发"级故障）。
//   B 门关（对手满血）：40 回合零麻痹（确定性关）。
//   C 属性技：门开也不触发（used_attribute 门，确定性）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kYanFan = 10037;   // 燕返：威力 60（直接攻击载体）
constexpr int kGaoSu = 20017;    // 高速移动：属性技
constexpr int kParalysis = static_cast<int>(AbnormalStatusId::Paralysis);

SoulMark make_mark() {
    SoulMark sm(296, "K296", "test", EffectArgs({1, 50, 6}));
    sm.monster_id = 2585;
    return sm;
}

// 0 方带 mark 攻 rounds 回合（1 方 NONE；foe_hp_pct 对手血量百分比，
// attribute_only=真出手但全属性技载体），返回对手麻痹落地回合数（查后 dispel 独立观测）。
int probe_paralysis(int foe_hp_pct, bool attribute_only, int rounds) {
    ElfPet holder = scenario::make_pet(
        1, "净天云神",
        attribute_only ? std::array<int, 5>{kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu}
                       : std::array<int, 5>{kYanFan, kYanFan, kYanFan, kYanFan, kYanFan},
        /*speed=*/300, /*atk=*/5000, /*hp=*/1000000, make_mark());
    ScenarioBattle b(scenario::make_party(holder),
                     scenario::make_party(scenario::make_pet(
                         2, "沙包", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                         /*speed=*/100, /*atk=*/10, /*hp=*/1000000)));
    BattleContext& ctx = b.ctx();
    sbs_srand(20261007);
    b.start();
    if (foe_hp_pct < 100) {
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp =
            ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp * foe_hp_pct / 100;
    }
    int wins = 0;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r % 5);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        if (ctx.has_active_abnormal_status(1, kParalysis)) {
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

    // ══ A 门开 40 回合：落地 ≤6 ═══════════════════════════════════════════
    {
        const int wins = probe_paralysis(20, 0, 40);
        std::cout << "  [A] 40 回合麻痹落地 " << wins << "（期望 ~2）" << std::endl;
        CHECK(wins <= 6, "A 6% 稀疏档：40 回合落地 ≤6");
        CHECK(wins >= 1, "A 稀疏档应有落地（种子钉死应≥1）");
    }

    // ══ B 门关（对手满血）：零麻痹 ════════════════════════════════════════
    CHECK_EQ(probe_paralysis(100, 0, 40), 0, "B 对手满血：零麻痹");

    // ══ C 属性技真出手：门开也零麻痹（used_attribute 门，确定性）══════════
    CHECK_EQ(probe_paralysis(20, /*attribute_only=*/true, 20), 0, "C 属性技：零麻痹");

    std::cout << "scenario_3520 PASS (K210 296 低血攻附麻痹)" << std::endl;
    return 0;
}
