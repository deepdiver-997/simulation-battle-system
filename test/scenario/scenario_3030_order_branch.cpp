// 场景 3030 —— K169 先/后手双分支族（393，soul_lib/k169_order_branch.cpp）
//（2026-10-06 起草 / 2026-10-07 回收收尾，号段 3030-3039 实配 T3030-T3032 三块）
//
// 断言口径：
//   - 393 奥古斯特 2819 [150,100]：先手回合 → 己方攻击 +100 固定（DETECT 差账精确）；
//     后手回合 → RC 回 150 体力（hp 直查）。
//   - 393 地葬界王 2991 [200,200]：两行同构互锚（行≠宏，args 驱动数值）。
//   - 速度布局：fast 局我 300/对手 101 → 恒先手（每回合攻 +attach）；slow 局
//     我 100/对手 101 → 恒后手（每回合 RC 回 heal）。
//   - attach 走 DETECT 相位（final 已定、hp 未扣、减伤之后）→ 差账精确；
//     heal 走 heal_amount（RC 时点，round 已完成、无并发伤害）→ 精确。
#include <array>
#include <iostream>
#include <utility>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击载体（atk=1 → 基线约 2 点/击）
constexpr int kAttachAugust = 100;   // 393 奥古斯特 db[1]
constexpr int kHealAugust = 150;     // 393 奥古斯特 db[0]
constexpr int kAttachDizang = 200;   // 393 地葬界王 db[1]
constexpr int kHealDizang = 200;     // 393 地葬界王 db[0]

ElfPet mark_pet(int monster_id, std::pair<int, int> db_args, int speed,
                const std::array<int, 5>& skills) {
    SoulMark sm(393, "K393", "test", EffectArgs({db_args.first, db_args.second}));
    sm.monster_id = monster_id;
    ElfPet p = make_pet(1, "印主", {skills[0], skills[1], skills[2], skills[3], skills[4]},
                        speed, /*atk=*/1, /*hp=*/99999, std::move(sm));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(const std::array<int, 5>& skills, int speed = 101) {
    ElfPet p = make_pet(2, "对手", {skills[0], skills[1], skills[2], skills[3], skills[4]},
                        speed, /*atk=*/1, /*hp=*/99999);
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

    // ══ T3030 先手分支（奥古斯特 [150,100]）：每回合己方攻击 +100 固定（3 回合差账）══
    {
        ScenarioBattle b(make_party(mark_pet(2819, {kHealAugust, kAttachAugust}, 300,
                                              {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int round = 1; round <= 3; ++round) {
            const int foe0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int loss = foe0 - ctx.getPet(1).hp;
            CHECK_EQ(loss, 2 + kAttachAugust,
                     "T3030 393：先手回合攻击 +100 固定（逐回合）");
        }
    }

    // ══ T3031 后手分支（奥古斯特 [150,100]）：每回合 RC 回 150（我 speed 100 后手）══
    {
        ScenarioBattle b(make_party(mark_pet(2819, {kHealAugust, kAttachAugust}, 100,
                                              {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 5000;
        int my0 = ctx.getPet(0).hp;
        for (int round = 1; round <= 3; ++round) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int gain = ctx.getPet(0).hp - my0;
            CHECK_EQ(gain, kHealAugust - 2,
                     "T3031 393：后手回合回 150（净 +148，扣对手拍打 2 点）");
            my0 = ctx.getPet(0).hp;
        }
    }

    // ══ T3032 第二行同构（地葬界王 [200,200]）：先手攻附 200 ═══════════════════
    {
        ScenarioBattle b(make_party(mark_pet(2991, {kHealDizang, kAttachDizang}, 300,
                                              {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int foe0 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(foe0 - ctx.getPet(1).hp, 2 + kAttachDizang,
                 "T3032 393 地葬界王：先手攻附 200（args 驱动）");
    }

    std::cout << "scenario_3030 PASS (K169 先/后手双分支族)" << std::endl;
    return 0;
}
