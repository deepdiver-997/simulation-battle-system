#include <utils/sbs_rand.h>
// 场景 240：混沌魔君索伦森 3414 第五技「混沌灭世决 30995」缺失效果 1133
//（moves_lib/d5_D8.cpp，夜跑 night-2，2026-09-28）
//
// 1133：对手每有 1 项能力等级与自身相同（6 槽逐槽比对，0==0 亦算，字面口径记档待拍板）
// 则附加 120 点固定伤害。断言口径：对手掉血 = raw_attack_damage（裸伤台账）+ 期望固伤。
//
// 断言组：
//   T1 全零对全零：6 槽全同 → 固伤 720（字面口径的基准强度）。
//   T2 一槽不同：自身攻+2、对手无强化 → 5 槽相同 → 固伤 600。
//   T3 链序锁定：双方同 3 槽+2（先验 6 同），但链内 1217 先消强 → 对手归零后
//      只剩自身 3 个零槽同 → 固伤 360（证明 1133 读的是消强后的等级）。
//   T4 对照：拍打不带链 → 无固伤。
//
// 确定性：30937/拍打 crit_rate=0、必中、同系克制 1.0；伤害浮动含在裸伤台账内。
// 链内 1218（未击败→下2回合先制+2）照常触发，与本断言无关。
#include <array>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHunShi = 30995;  // 混沌灭世决：混沌特攻 160、必中、effect [1217,1218,1133]
constexpr int kPaDa = 10004;    // 拍打：物攻 40、无效果

ElfPet sorensen(int speed) {
    ElfPet p = scenario::make_pet(0, "混沌魔君索伦森", {kHunShi, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/150, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe(int speed) {
    ElfPet p = scenario::make_pet(1, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                  speed, /*atk=*/10, /*hp=*/9999);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void set_levels(BattleContext& ctx, int side, const std::array<int, 3>& first3) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        ctx.ability_levels[side][i] = i < 3 ? first3[static_cast<std::size_t>(i)] : 0;
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 全零对全零：6 槽全同 → 固伤 720 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T1 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw - 720,
                 "T1 核心：6 槽全同（含 0==0）→ 6×120=720 固伤");
        std::cout << "  [T1] 全零 6 同固伤 720 通过（裸伤 " << raw << "）\n";
    }

    //========== T2 一槽不同：自身攻+2 → 5 同 → 固伤 600 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_levels(ctx, 0, {2, 0, 0});   // 自身攻+2，其余零
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T2 前提：攻击伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw - 600,
                 "T2 核心：攻槽 2≠0 → 5 槽相同 → 5×120=600 固伤");
        std::cout << "  [T2] 一槽不同固伤 600 通过\n";
    }

    //========== T3 链序锁定：1217 先消强 → 1133 读消强后等级 → 固伤 360 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        set_levels(ctx, 0, {2, 2, 2});   // 自身 3 槽 +2
        set_levels(ctx, 1, {2, 2, 2});   // 对手同 3 槽 +2（先验 6 槽全同）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T3 前提：攻击伤害落地");
        CHECK(ctx.ability_levels[1][0] == 0 && ctx.ability_levels[1][1] == 0
                  && ctx.ability_levels[1][2] == 0,
              "T3 前提：链内 1217 已消掉对手强化");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw - 360,
                 "T3 核心：消强后仅自身 3 个零槽相同 → 3×120=360（数据序=执行序）");
        std::cout << "  [T3] 消强后比对固伤 360 通过\n";
    }

    //========== T4 对照：拍打不带链 → 无固伤 ==========
    {
        sbs_srand(20260928);
        ScenarioBattle b(scenario::make_party(sorensen(/*speed=*/200)),
                         scenario::make_party(foe(/*speed=*/100)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK(raw > 0, "T4 前提：拍打伤害落地");
        CHECK_EQ(ctx.getPet(1).hp, 9999 - raw,
                 "T4 核心：非 30995 攻击无固伤链");
        std::cout << "  [T4] 拍打无链对照 通过\n";
    }

    std::cout << "scenario_240 all passed" << std::endl;
    return 0;
}
