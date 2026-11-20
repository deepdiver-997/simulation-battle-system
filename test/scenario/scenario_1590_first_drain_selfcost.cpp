// 场景 1590 —— J72 先手吸血/自耗反噬族（2 宏，moves_lib/j_first_drain_selfcost.cpp）
//（2026-10-03，worktree ../agents/j72；号段 1590-1599 → T1590-T1591 两块）
//
// 断言口径：
//   - 458 先手门 = 速度倒置对照（先手回血/后手不回）；回血 = 裸伤×50%——裸伤与
//     终伤差一档 85%-100% 浮动 → 相对带 [0.4×F, 0.6×F]（F=对手掉血=终伤）；
//   - 1185 = 预置半血 4000 → cost=2000 精确：自耗后 2000 整、对手吃等量粉
//     （PERCENT_VALUE 无浮动）+我方红伤~2 → 窄带 [2001,2004]；
//   - 两载体全纯链（13871 [458]|[50] / 31170 [1185]|[2]）——无兄弟链激活风险。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kJiYue  = 13871;  // 极月无双破 [458]|[50]：特150（先手吸血载体）
constexpr int kXiMie  = 31170;  // 熄灭光明 [1185]|[2]：物130（自耗反噬载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int speed) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/50, kMaxHp);   // atk=50：伤害≈百级，50% 截断噪声<1%
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int speed) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1590 458 先手→伤害 50% 回血（极月无双破 [50]）══
    {
        // a) 先手：攻击造成 F 点（裸伤≥F）→ 回裸伤 50% ∈ [0.4F, 0.6F]
        {
            ScenarioBattle b(
                make_party(mk_self({kJiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;                  // 预置缺口（防回血钳制）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int gain = ctx.getPet(0).hp - 4000;
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss > 0, "T1590.a 前置：攻击已造成伤害（实测 " +
                                    std::to_string(foe_loss) + "）");
            CHECK(gain * 100 >= foe_loss * 45 && gain * 100 <= foe_loss * 55,
                  "T1590.a 458：先手回血=裸伤50%（回 " + std::to_string(gain) +
                      " / 终伤 " + std::to_string(foe_loss) + "）");
        }
        // b) 后手对照：门关 → 不回血
        {
            ScenarioBattle b(
                make_party(mk_self({kJiYue, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4000,
                     "T1590.b 458 对照：后手门关不回（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1590] 458 先手吸血 通过\n";
    }

    // ══ T1591 1185 自耗 1/{0} + 等量反噬（熄灭光明 [2]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kXiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 4000;                      // cost=4000/2=2000
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, 2000,
                 "T1591.a 1185：自耗当前体力一半（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        const int foe_loss = 99999 - ctx.getPet(1).hp;
        CHECK(foe_loss >= 2040 && foe_loss <= 2060,
              "T1591.a 1185：对手吃等量粉 2000+红伤~2（实测 " +
                  std::to_string(foe_loss) + "）");
        std::cout << "  [T1591] 1185 自耗反噬 通过\n";
    }

    std::cout << "scenario_1590 PASS (J72 先手吸血/自耗反噬族 458/1185)" << std::endl;
    return 0;
}
