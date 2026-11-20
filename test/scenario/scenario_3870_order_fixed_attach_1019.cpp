// 场景 3870 —— K243 先后手双段固定伤族（1019，soul_lib/k243_order_fixed_attach.cpp）
//（2026-10-13，夜跑 k-prod-6；号段 3870-3879 → T3870 一块四例）
//
// 断言口径（附加值为面板/最大体力的精确公式值，门为严格大于）：
//   - a 先手正例：对手 max 99999 → 附加 24999 > 200 → 回复恰 33333（预压血量）；
//     对手掉血 = 红伤带[62,99] + 24999 ∈ (25060,25100)。
//   - b 先手门负例：对手 max 800 → 附加 200 不高于 200 → 无回复（自身原地）；
//     对手掉血 = 红伤带 + 200 ∈ (261,300)。
//   - c 后手门负例：atk 300 → 附加 150 不高于 250 → 无吸取；对手掉血 = 红伤带 + 150
//     ∈ (211,250)；自身原地。
//   - d 后手正例：atk 600 → 附加 300 > 250 → 吸取恰 24999（deal+heal 等量）；
//     自身 +24999；对手掉血 = 红伤带[124,198] + 附加 300 + 24999 ∈ (25420,25500)。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击技 filler

SoulMark make_mark() {
    SoulMark sm(1019, "K243 魂印", "test", EffectArgs(std::vector<int>{4, 200, 3, 50, 250, 4, 1}));
    sm.monster_id = 3417;   // 埃兰
    return sm;
}

ElfPet mark_pet(int speed, int atk) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, atk, 99999, make_mark());
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int hp, int speed = 100) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                        speed, /*atk=*/1, hp);
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

    // ══ T3870.a 先手正例：附加 24999 → 门开 → 回复 33333 ══
    {
        ScenarioBattle b(make_party(mark_pet(300, 300)), make_party(foe_pet(99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 66000;   // 预压血量（回复 33333 需足额余量，SOP §五.2）
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 99999 / 3, "T3870.a 先手回复 = max/3");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 25080 && foe_delta < 25110,
              "T3870.a 对手掉血 = 红伤带 + 附加 24999");
        std::cout << "  [T3870.a] 先手正例 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T3870.b 先手门负例：对手 max 800 → 附加 200 恰在门限（不高于）→ 无回复 ══
    {
        ScenarioBattle b(make_party(mark_pet(300, 300)), make_party(foe_pet(800)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3870.b 附加 200 不高于门 → 无回复");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 285 && foe_delta < 306, "T3870.b 对手掉血 = 红伤带 + 附加 200");
        std::cout << "  [T3870.b] 先手门负例 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T3870.c 后手门负例：atk 300 → 附加 150 ≤ 250 → 无吸取 ══
    {
        ScenarioBattle b(make_party(mark_pet(100, 300)), make_party(foe_pet(99999, 300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 99500;
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp, hp0, "T3870.c 附加 150 不高于门 → 无吸取");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 235 && foe_delta < 255, "T3870.c 对手掉血 = 红伤带 + 附加 150");
        std::cout << "  [T3870.c] 后手门负例 通过（对手掉血=" << foe_delta << "）\n";
    }

    // ══ T3870.d 后手正例：atk 600 → 附加 300 → 吸取 24999 ══
    {
        ScenarioBattle b(make_party(mark_pet(100, 600)), make_party(foe_pet(99999)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 66000;   // 预压血量（吸取回复 24999 需余量）
        const int hp0 = ctx.getPet(0).hp;
        const int foe_hp0 = ctx.getPet(1).hp;
        one_round(b);
        CHECK_EQ(ctx.getPet(0).hp - hp0, 99999 / 4, "T3870.d 后手吸取 = 对手 max/4 等量回复");
        const int foe_delta = foe_hp0 - ctx.getPet(1).hp;
        CHECK(foe_delta > 25460 && foe_delta < 25515,
              "T3870.d 对手掉血 = 红伤带(atk600)[124,198] + 附加 300 + 吸取 24999");
        std::cout << "  [T3870.d] 后手正例 通过（对手掉血=" << foe_delta << "）\n";
    }

    std::cout << "scenario_3870 PASS (K243 先后手双段固定伤族)" << std::endl;
    return 0;
}
