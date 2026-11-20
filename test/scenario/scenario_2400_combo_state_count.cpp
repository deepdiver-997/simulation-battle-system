// 场景 2400 —— J142 连击面状态计数族（1108，moves_lib/j_combo_state_count.cpp）
//（2026-10-04，worktree ../agents/j142；号段 2400-2409 → T2400 一块五档）
//
// 断言口径：纯链载体 深海多重击 33483 [1108]|[1,1]（特25）；断言走
// ctx.resolvedDamage.hitCount（确定值，不吃 85%~100% 浮动）：
//   无状态=1 段（"连击数+N"字面：无状态普通单段）→ 自身 +1 档=2 → 对手 -1 档=2 →
//   双方各 1 档=3 → 同槽 +2 仍 1 种类=2（按种类计不按等级和——J13/J21 判据）。
// 状态 setup 走 stat_change 直授（记档：对手 -1 用 stat_drop 通道、免弱空场必落）。
#include <functional>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShenHai = 33483;   // 深海多重击：特25 纯链 [1108] → [1,1]
constexpr int kPaDa    = 10004;   // 拍打：filler

constexpr int kSlotAtk = 0;       // 引擎槽：0攻（对手弱化档用）

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

// 每档独立对局：setup → 施放深海多重击 → 断言 hitCount。
int hits_with(BattleContext& ctx) { return ctx.resolvedDamage.hitCount; }

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2400 1108 状态种类 → 连击段数（五档）══
    auto run = [&](const char* tag, int expect,
                   const std::function<void(BattleContext&)>& setup) {
        ScenarioBattle b(make_party(self_pet({kShenHai, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        if (setup) {
            setup(ctx);
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(hits_with(ctx), expect, tag);
    };

    run("T2400.a 1108：无状态 → 1 段（普通单段）", 1, nullptr);
    run("T2400.b 1108：自身 1 种提升 → 2 段", 2, [](BattleContext& c) {
        REQUIRE(stat_change(&c, 0, kSlotAtk, 1) == StatChangeResult::SUCCESS);
    });
    run("T2400.c 1108：对手 1 种弱化 → 2 段", 2, [](BattleContext& c) {
        REQUIRE(stat_drop(&c, 1, kSlotAtk, 1, 0) == StatDropResult::SUCCESS);
    });
    run("T2400.d 1108：自身提升+对手弱化各 1 种 → 3 段", 3, [](BattleContext& c) {
        REQUIRE(stat_change(&c, 0, kSlotAtk, 1) == StatChangeResult::SUCCESS);
        REQUIRE(stat_drop(&c, 1, kSlotAtk, 1, 0) == StatDropResult::SUCCESS);
    });
    run("T2400.e 1108：同槽 +2 仍 1 种类（按种类不按等级和）→ 2 段", 2,
        [](BattleContext& c) {
            REQUIRE(stat_change(&c, 0, kSlotAtk, 2) == StatChangeResult::SUCCESS);
        });
    std::cout << "  [T2400] 1108 状态计数连击 通过\n";

    std::cout << "scenario_2400 PASS (J142 连击面状态计数族)" << std::endl;
    return 0;
}
