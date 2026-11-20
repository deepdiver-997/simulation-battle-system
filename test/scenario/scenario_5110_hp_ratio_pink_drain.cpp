// 场景 5110 —— J189 体力比例门粉伤/吸取族（1136 / 1137，moves_lib/j_hp_ratio_pink_drain.cpp）
//（2026-10-22，worktree ../agents/j189；号段 5110-5119）
//
// 断言口径：
//   - 1136（锐爪突袭 34387 链 [179,1025,1136]|[50,3,2,3]）：自身体力 4000 > max/2=2500
//     → 粉伤 = 当前体力/3 = 1333。审计槽 990113600 精确断言 1333；对手掉血带
//     [1333,1345]（粉伤 + atk=0 的保底红伤 ≤ 数点）。对照 hp=2000（不 >2500）→ 不掉粉伤。
//   - 1137（弑界之霆 26762 属技 [191,1390,1137,1473]|[4,100,0,2,100,3,3,2]）：
//     自身体力 1000 < max/3=1666 → 吸取对手 max/3=33333。属技无红伤 → 对手
//     hp 99999→66666 精确；自身 1000→5000（+33333 钳满）。对照 hp=4000（不 <1666）
//     → 对手/自身血量均不变。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kRuiZhua  = 34387;  // [179,1025,1136]|[50,3,2,3]（1136 [2,3]）
constexpr int kShiJie   = 26762;  // [191,1390,1137,1473]|[4,100,0,2,100,3,3,2]（1137 [3,3]）
constexpr int kAudit1136 = 990113600;
constexpr int kAudit1137 = 990113700;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill, int hp, int atk = 0) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    p.hp = hp;   // 压当前体力（max 仍是 5000）
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5110 1136 高体比例粉伤 ══
    {
        {   // (a) hp 4000 > max/2 → 粉伤 4000/3=1333
            ScenarioBattle b(make_party(mk_self(kRuiZhua, /*hp=*/4000)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(stor(ctx, kAudit1136), 1333,
                     "T5110.a 1136：审计槽粉伤 = 4000/3 = 1333（实测 " +
                         std::to_string(stor(ctx, kAudit1136)) + "）");
            const int loss = foe_before - ctx.getPet(1).hp;
            CHECK(loss >= 1333 && loss <= 1345,
                  "T5110.a 1136：对手掉血 = 粉伤 1333 + 保底红伤（实测 " +
                      std::to_string(loss) + "）");
        }
        {   // (b) 对照 hp 2000 不高于 max/2 → 无粉伤
            ScenarioBattle b(make_party(mk_self(kRuiZhua, /*hp=*/2000)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(stor(ctx, kAudit1136), 0,
                     "T5110.b 1136 对照：低体不触发粉伤（审计槽为 0）");
            const int loss = foe_before - ctx.getPet(1).hp;
            CHECK(loss <= 5, "T5110.b 1136 对照：仅保底红伤（实测 " +
                                 std::to_string(loss) + "）");
        }
        std::cout << "  [T5110] 1136 高体比例粉伤 + 对照 通过\n";
    }

    // ══ T5110 1137 低体吸取 ══
    {
        {   // (a) hp 1000 < max/3=1666 → 吸取对手 max/3=33333
            ScenarioBattle b(make_party(mk_self(kShiJie, /*hp=*/1000)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(stor(ctx, kAudit1137), 33333,
                     "T5110.c 1137：审计槽吸取基数 = 99999/3 = 33333（实测 " +
                         std::to_string(stor(ctx, kAudit1137)) + "）");
            CHECK_EQ(ctx.getPet(1).hp, 66666,
                     "T5110.c 1137：对手 99999→66666（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 5000,
                     "T5110.c 1137：自身 1000+33333 钳满 5000（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        {   // (b) 对照 hp 4000 不低于 max/3 → 不吸取
            ScenarioBattle b(make_party(mk_self(kShiJie, /*hp=*/4000)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(stor(ctx, kAudit1137), 0,
                     "T5110.d 1137 对照：高体不触发吸取（审计槽为 0）");
            CHECK_EQ(ctx.getPet(1).hp, 99999,
                     "T5110.d 1137 对照：对手体力不变（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 4000,
                     "T5110.d 1137 对照：自身体力不变（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T5110] 1137 低体吸取 + 对照 通过\n";
    }

    std::cout << "scenario_5110 PASS (J189 体力比例门粉伤/吸取族)" << std::endl;
    return 0;
}
