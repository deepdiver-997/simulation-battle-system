// 场景 3970 —— K247 对手烧伤门先制+威力翻倍族（739，soul_lib/k247_burn_gate_preempt_power.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 3970-3979 → T3970 一块三例）
//
// 断言口径（威力 ×2 下界 1.275B > 常规上界 B，带天然分离）：
//   - a 预铺对手烧伤 → R1 攻击 d>120（×2 带 [124,198] vs 常规 [62,99]）。
//   - b 先制翻转：印主 speed 100 < 对手 101 → 无烧时对手先手（right=SEER_ROBOT_2）；
//     预铺烧伤 → 先制+1 → 印主先手（right=SEER_ROBOT_1）（2830 right 读法）。
//   - c 无烧伤门负例：不铺烧伤 → d ∈ 常规带 + right=SEER_ROBOT_2。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

SoulMark make_mark() {
    SoulMark sm(739, "K247 魂印", "test", EffectArgs(std::vector<int>{2, 1}));
    sm.monster_id = 3066;   // 鬼焰神君陆逊
    return sm;
}

ElfPet mark_pet(int speed) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/300, 99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/1, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3970.a 对手烧伤 → 攻击威力 ×2 ══
    {
        ScenarioBattle b(make_party(mark_pet(300)), make_party(foe_pet(100)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 1, kBurn, 5, 0), ApplyAnomalyResult::SUCCESS,
                 "T3970.a 预铺对手烧伤");
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int d1 = foe_hp0 - ctx.getPet(1).hp;
        CHECK(d1 > 120, "T3970.a 烧伤门 → 威力 ×2（[124,198]）");
        std::cout << "  [T3970.a] 烧伤门威力翻倍 通过（d1=" << d1 << "）\n";
    }

    // ══ T3970.b 烧伤门 → 先制+1 翻转先手（speed 100 vs 101）══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(foe_pet(101)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 1, kBurn, 5, 0), ApplyAnomalyResult::SUCCESS,
                 "T3970.b 预铺对手烧伤");
        one_round(b);
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3970.b 先制+1 → 印主翻转先手（2830 right 读法）");
        std::cout << "  [T3970.b] 烧伤门先制翻转 通过\n";
    }

    // ══ T3970.c 无烧伤门负例 ══
    {
        ScenarioBattle b(make_party(mark_pet(100)), make_party(foe_pet(101)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        const int d1 = foe_hp0 - ctx.getPet(1).hp;
        CHECK(d1 > 85 && d1 < 104, "T3970.c 无烧伤 → 常规威力");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T3970.c 无烧伤 → 对手先手（101>100）");
        std::cout << "  [T3970.c] 无烧伤负例 通过\n";
    }

    std::cout << "scenario_3970 PASS (K247 对手烧伤门先制+威力翻倍族)" << std::endl;
    return 0;
}
