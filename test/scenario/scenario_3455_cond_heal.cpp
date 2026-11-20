// 场景 3455 —— K206 异常负槽回血族（295，soul_lib/k206_cond_heal.cpp）
//（2026-10-07，worktree ../agents/k206；号段 3455-3464 → T3455-T3457 三块）
//
// 断言口径（ROUND_START 在提交后跑——铺状态时机充裕）：
//   负槽档（直铺 -1 攻，无异常无 tick）→ 净 +150 精确；
//   异常档（直铺烧伤）→ 方向断言（烧 tick 公式不定，净 >0）；
//   双净档 → 0。
#include <array>
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);

ElfPet mark_pet(int hp = 99999) {
    std::vector<int> args{0, 0};
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, hp,
                        /*soul_mark=*/SoulMark(295, "K206 魂印", "",
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

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3455 负槽档：-1 攻 → 回合首回 150（无 tick，净精确）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        ctx.ability_levels[0][0] = -1;
        const int my0 = ctx.getPet(0).hp;
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).hp - my0, 150, "T3455 295：负槽档 → +150 精确");
        std::cout << "  [T3455] 负槽档回血 通过\n";
    }

    // ══ T3456 异常档：烧伤 → 回血方向成立（小 max 压 tick；回血受 200 封顶）══
    {
        ScenarioBattle b(make_party(mark_pet(/*hp=*/200)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 100;
        REQUIRE(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/2, /*actor=*/1));
        const int my0 = ctx.getPet(0).hp;
        idle_round(b);
        CHECK(ctx.getPet(0).hp > my0, "T3456 295：异常档 → 回血方向成立（净=150-tick，封顶 200）");
        std::cout << "  [T3456] 异常档回血 通过\n";
    }

    // ══ T3457 双净档：无异常无负槽 → 不回 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        const int my0 = ctx.getPet(0).hp;
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).hp, my0, "T3457 295：双净档 → 不回血");
        std::cout << "  [T3457] 双净档对照 通过\n";
    }

    std::cout << "scenario_3455 PASS (K206 异常负槽回血族)" << std::endl;
    return 0;
}
