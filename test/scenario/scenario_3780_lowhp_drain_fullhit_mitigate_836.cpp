// 场景 3780 —— K234 低体吸取+满体受击减伤族（836，soul_lib/k234_lowhp_drain_fullhit_mitigate_836.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3780-3789 → T3780 一块六例）
//
// 断言口径（比例带精确分离；85%~100% 浮动下各档互不重叠）：
//   - a 低体吸取正例：hp 4000 < max/2 → 己方攻击命中 → 回复恰 max/3=33333
//     （预压血量防封顶），对手掉血 = 红伤带[62,99]+33333 ∈ (33390,33440)。
//   - b 低体门负例：hp 99500 ≥ max/2 → 无吸取（己方原地、对手掉血=红伤带）。
//   - f 门界（整数除法）：hp = max/2 = 49999 不低于 49999 → 不吸（严格小于口径）。
//   - c 满体三连击（row1 -50%）：R1 满体受击全额[62,99]+装填；R2 减伤半血
//     （[31,50]，与全额带分离）+击后非满体；R3 票已耗回全额[62,99]。
//   - d 满体门负例：非满受击不装填（连续两击都全额）。
//   - e row2 -65%：减伤带 = 红伤×0.35 ∈ [21,35]。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(836, "K234 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3211;   // 海瑟薇
    return sm;
}

ElfPet mark_pet(const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/300, /*atk=*/300, 99999, make_mark(row_args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        /*speed=*/100, atk, 99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType a0, BattleFsm::ActionType a1) {
    b.submit_operation(0, a0, a0 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.submit_operation(1, a1, a1 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row1{2, 3, 1, 50, 1};
    const std::vector<int> row2{2, 3, 1, 65, 1};

    // ══ T3780.a 低体吸取正例 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 4000;   // < max/2
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 99999 / 3, "T3780.a 回复 = 对手 max/3");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 33390 && foe_delta < 33440,
              "T3780.a 对手掉血 = 红伤带 + 吸取 33333");
        std::cout << "  [T3780.a] 低体吸取 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T3780.b 低体门负例 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;   // ≥ max/2
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3780.b 非低体不回复");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 85 && foe_delta < 104, "T3780.b 对手只吃红伤（无吸取，raw 带 [87,102]）");
        std::cout << "  [T3780.b] 低体门负例 通过\n";
    }

    // ══ T3780.f 门界（整数除法严格小于）══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99999 / 2;   // 49999 不低于 int 49999
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3780.f 恰在门限（≥ int max/2）不吸取");
        std::cout << "  [T3780.f] 门界严格小于 通过\n";
    }

    // ══ T3780.c 满体三连击：全额 → 减半 → 票耗回全额（row1 -50%）══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;   // 满体力开场
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d1 = hp0 - ctx.getPet(0).hp;
        CHECK(d1 > 85 && d1 < 104, "T3780.c R1 满体受击全额（装填）");
        const int hp1 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d2 = hp1 - ctx.getPet(0).hp;
        CHECK(d2 > 42 && d2 < 53, "T3780.c R2 下 1 次减伤 50%（raw[87,102]×0.5）");
        const int hp2 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d3 = hp2 - ctx.getPet(0).hp;
        CHECK(d3 > 85 && d3 < 104, "T3780.c R3 票已耗回全额（且击后非满体不再装填）");
        std::cout << "  [T3780.c] 满体三连击减伤 通过（" << d1 << "/" << d2 << "/" << d3 << "）\n";
    }

    // ══ T3780.d 满体门负例：非满受击不装填 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;   // 非满
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d1 = hp0 - ctx.getPet(0).hp;
        CHECK(d1 > 85 && d1 < 104, "T3780.d R1 非满体全额（不装填）");
        const int hp1 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d2 = hp1 - ctx.getPet(0).hp;
        CHECK(d2 > 85 && d2 < 104, "T3780.d R2 仍全额（无票可耗）");
        std::cout << "  [T3780.d] 满体门负例 通过\n";
    }

    // ══ T3780.e row2 -65%：减伤带 [21,35] ══
    {
        ScenarioBattle b(make_party(mark_pet(row2)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int hp1 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::NONE, BattleFsm::ActionType::SELECT_SKILL);
        const int d2 = hp1 - ctx.getPet(0).hp;
        CHECK(d2 > 29 && d2 < 37, "T3780.e row2 下 1 次减伤 65%（raw×0.35）");
        std::cout << "  [T3780.e] row2 减伤 65% 通过（d2=" << d2 << "）\n";
    }

    std::cout << "scenario_3780 PASS (K234 低体吸取+满体受击减伤族)" << std::endl;
    return 0;
}
