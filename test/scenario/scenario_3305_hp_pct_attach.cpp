// 场景 3305 —— K197 攻击附加对手体力百分比固伤族（259，soul_lib/k197_hp_pct_attach.cpp）
//（2026-10-07，worktree ../agents/k197；号段 3305-3314 → T3305-T3306 两块）
//
// 断言口径（raw 台账差账，单局精确）：
//   附加走 DETECT 相位 deal_pink_damage(FIXED)，红伤 hp 扣减在其后（管线先于
//   apply_resolved_damage）→ 对手失血 = raw[0] + floor(当前体力/10)。当前体力取
//   DETECT 时点现值（首攻前满血 → 9999；次攻按剩余体力重算 floor）。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;   // 撞击

ElfPet mark_pet() {
    std::vector<int> args{0, 0};   // 零参行：bind 占位
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(259, "K197 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void hit_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3305 首攻：失血 = raw + floor(99999/10)=9999 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        hit_round(b);
        const int loss = foe0 - ctx.getPet(1).hp;
        CHECK_EQ(loss, ctx.ws.raw_attack_damage[0] + 9999,
                 "T3305 259：首攻附加 10%×99999=9999 固定（raw 差账精确）");
        std::cout << "  [T3305] 满血档附加 通过\n";
    }

    // ══ T3306 次攻：当前体力现值重算 floor ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        hit_round(b);
        const int hp_after_r1 = ctx.getPet(1).hp;
        REQUIRE(hp_after_r1 > 0);
        const int foe1 = ctx.getPet(1).hp;
        hit_round(b);
        const int loss2 = foe1 - ctx.getPet(1).hp;
        CHECK_EQ(loss2, ctx.ws.raw_attack_damage[0] + hp_after_r1 / 10,
                 "T3306 259：次攻按当前体力现值 floor(hp/10) 重算");
        std::cout << "  [T3306] 现值重算 通过\n";
    }

    std::cout << "scenario_3305 PASS (K197 攻击附加对手体力百分比固伤族)" << std::endl;
    return 0;
}
