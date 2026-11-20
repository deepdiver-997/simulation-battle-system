// 场景 3360 —— K200 暴击吸血族（321，soul_lib/k200_crit_drain.cpp）
//（2026-10-07，worktree ../agents/k200；号段 3360-3369 → T3360-T3361 两块）
//
// 断言口径（净差账，单局精确）：
//   印主必暴（set_crit_sixteenths 16 档短路）→ 对手失血 = raw[0]，自身净回复
//   = floor(amount×30/100)（heal 封顶前血量压低）。非暴击档（rate 0）→ 不回血。
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

constexpr int kJi = 10001;

ElfPet mark_pet(bool must_crit) {
    // ⚠️ 方案 B（2026-10-21）：行参按 **DB args 原样**构造（不再补 {0,0} 占位）——
    //    实现侧走 db_at 读 owned_int_args 原序，与真实对局一致。
    std::vector<int> args{30, 0};   // row[30,0]
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/60, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(321, "K200 魂印", "",
                                               EffectArgs(std::move(args))));
    set_crit_sixteenths(p, must_crit ? 16 : 0);
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3360 必暴：回血 = floor(失血×30/100)（压低自身血量防封顶）══
    {
        ScenarioBattle b(make_party(mark_pet(/*must_crit=*/true)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;   // 压低：30% 回血不被 99999 封顶
        const int foe0 = ctx.getPet(1).hp;
        const int my0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int dealt = foe0 - ctx.getPet(1).hp;
        CHECK_EQ(ctx.getPet(0).hp - my0, dealt * 30 / 100,
                 "T3360 321：必暴 → 回血 = 伤害×30%（净差账精确）");
        std::cout << "  [T3360] 必暴吸血 通过\n";
    }

    // ══ T3361 非暴击对照：零暴击率 → 不回血 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*must_crit=*/false)),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        // 非暴击 → 无吸血：自身血量不受自己出手影响（对手 NONE 无来伤）
        CHECK_EQ(ctx.getPet(0).hp, my0, "T3361 321：非暴击 → 不回血");
        std::cout << "  [T3361] 非暴击对照 通过\n";
    }

    std::cout << "scenario_3360 PASS (K200 暴击吸血族)" << std::endl;
    return 0;
}
