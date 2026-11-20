#include <utils/sbs_rand.h>
// 场景 310：洪荒·列奥尼达 4125 双第五技缺失效果 795、1327
//（moves_lib/d5_D15.cpp，夜跑 night-2，2026-09-28）
//
// 795（34593 切片 [20, 100]）：每次使用当回合攻击伤害 +20%，上限 +100%。
// 1327（38247 切片 [300, 3]）：攻击伤害 < 300 → 吸对手 1/3 最大体力（3333）+ 等量护盾。
// 断言口径：对手掉血 = 裸伤台账 ×(1+增伤)（+1327 的吸体等式）；盾吸收对手反击后按
// 400-被耗量 / 3333-被耗量 断言。
//
// 断言组：
//   TA 795 梯度：连续 5 次使用 34593，逐回合掉血 = 裸伤×(1.2/1.4/1.6/1.8/2.0)（±1）。
//   TC 1327 触发：38247（威力 85）伤害 < 300 → 对手掉血 = 裸伤 + 3333、
//      自身回 3333、盾 3333-被反击消耗。
//   TD 1327 反例：高攻使伤害 ≥ 300 → 不吸不盾（掉血 = 裸伤）。
//
// 确定性：34593/38247/拍打 crit_rate=0、必中；同系克制 1.0；链内 760/797/1249/1044/39
// 对白板无强化对手均无行为；伤害浮动含在裸伤台账内。38247 自带先制+3（先手）。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kSuiGu = 34593;   // 王·遂古之初：物攻 160、必中、effect [760,797,795,1249]
constexpr int kHuangXu = 38247; // 荒墟辟易：物攻 85、先制+3、effect [699,697,1044,39,1327]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果
constexpr int kDrain = 3333;    // 9999/3

ElfPet leonidas(int speed, int fifth, int atk = 150) {
    ElfPet p = scenario::make_pet(4125, "洪荒·列奥尼达", {fifth, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed, int atk = 10) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, atk, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void drive_round(ScenarioBattle& b, int slot0, int slot1) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, slot1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== TA 795 梯度：5 次使用 → 增伤 20/40/60/80/100% ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(leonidas(/*speed=*/200, kSuiGu)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 1; r <= 5; ++r) {
            const int before = ctx.getPet(1).hp;
            drive_round(b, 0, 0);
            const int raw = ctx.ws.raw_attack_damage[0];
            const int loss = before - ctx.getPet(1).hp;
            const int expect = raw * (100 + 20 * r) / 100;   // 通用增伤 +r% 的整数口径
            CHECK(loss >= expect - 1 && loss <= expect + 1,
                  "TA 核心：第 " << r << " 次使用增伤 " << 20 * r << "%（loss=" << loss
                                 << " expect=" << expect << " raw=" << raw << "）");
        }
        std::cout << "  [TA] 795 梯度 20/40/60/80/100 通过\n";
    }

    //========== TC 1327 触发：伤害 < 300 → 吸 1/3 最大体力 + 等量护盾 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(leonidas(/*speed=*/200, kHuangXu)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 5000;   // 预留吸血空间（回 3333 不触顶）
        drive_round(b, 0, 0);      // 荒墟辟易先制+3 先手；38247 威力 85 → 伤害 < 300
        const int raw = ctx.ws.raw_attack_damage[0];
        const int foe_hit = ctx.ws.raw_attack_damage[1];
        CHECK(raw < 300, "TC 前提：攻击伤害低于 300");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw - kDrain,
                 "TC 核心：吸取对手最大体力 1/3（3333 百分比伤害）");
        // 对手的后手反击被**刚落地的等量护盾**吸收（护盾先于 HP 扣减）：
        // 自身 HP = 5000 + 3333 全额回复，扣减体现在护盾余额上。
        CHECK_EQ(ctx.getPet(0).hp, 5000 + kDrain,
                 "TC 核心：吸取等量回复自身（反击由护盾吸收）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), kDrain - foe_hit,
                 "TC 核心：等量护盾 3333（被对手反击消耗后）");
        std::cout << "  [TC] 低伤吸取 + 等量护盾 通过（裸伤 " << raw << "）\n";
    }

    //========== TD 1327 反例：伤害 ≥ 300 → 不吸不盾 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(leonidas(/*speed=*/200, kHuangXu,
                                                       /*atk=*/3000)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_round(b, 0, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw >= 300, "TD 前提：攻击伤害不低于 300");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw, "TD 核心：无吸体（掉血=裸伤）");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "TD 核心：无护盾");
        std::cout << "  [TD] 伤害达阈不触发 通过\n";
    }

    std::cout << "scenario_310 all passed" << std::endl;
    return 0;
}
