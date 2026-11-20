// 场景 5160 —— J194 先手异反/回血压制族（1295 / 2116）
//（2026-10-22，worktree ../agents/j194；号段 5160-5169）
//
// 断言口径：
//   - 1295（圣子之威 35704 纯链 [1295]|[3]）：R1 先手施放 → Modern 免异常票 + REFLECT。
//     随后对**自身**施加烧伤(2) → 免疫不落地，反弹给对手（对手获得烧伤）。
//     对照：不施放 → 自身吃烧伤、对手无烧伤。
//   - 2116（沉眠 37671 纯链 [2116]|[1,50]）：R1 先手施放 → 对手 heal_mod_pct=-50。
//     drive 后把对手 hp 重置 4000，heal_amount(100) → 实际 50（hp 4050）。
//     对照：不施放 → heal 100 全额（hp 4100）。
//   - 两者都是绝对量断言；先手由 speed 300 vs 100 保证。
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

constexpr int kPaiDa   = 10004;  // 拍打：物 40（填充刀）
constexpr int kShengZi = 35704;  // [1295]|[3]（先手 3 回合免疫反弹异常）
constexpr int kChenMian = 37671; // [2116]|[1,50]（先手 1 回合对手回血 -50%）
constexpr int kBurn    = 2;      // 烧伤（AbnormalStatusId::Burn）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5160 1295 先手免疫并反弹异常 ══
    {
        {   // (a) 先手施放 → 对自身施加烧伤被免疫并反弹给对手
            ScenarioBattle b(make_party(mk_self(kShengZi)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            (void)apply_anomaly(&ctx, /*target=*/0, kBurn, /*duration=*/2, /*actor=*/1);
            CHECK(!ctx.has_active_abnormal_status(0, kBurn),
                  "T5160.a 1295：自身免疫烧伤");
            CHECK(ctx.has_active_abnormal_status(1, kBurn),
                  "T5160.a 1295：烧伤反弹给对手");
        }
        {   // (b) 对照：不施放 → 自身吃烧伤、对手无烧伤
            ScenarioBattle b(make_party(mk_self(kShengZi)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            (void)apply_anomaly(&ctx, /*target=*/0, kBurn, /*duration=*/2, /*actor=*/1);
            CHECK(ctx.has_active_abnormal_status(0, kBurn),
                  "T5160.b 1295 对照：未施放 → 自身吃烧伤");
            CHECK(!ctx.has_active_abnormal_status(1, kBurn),
                  "T5160.b 1295 对照：对手无烧伤");
        }
        std::cout << "  [T5160] 1295 先手免疫反弹异常 + 对照 通过\n";
    }

    // ══ T5160 2116 先手 → 对手回血 -{1}% ══
    {
        {   // (a) 先手施放 → heal 100 → 实际 50
            ScenarioBattle b(make_party(mk_self(kChenMian)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            ctx.getPet(1).hp = 4000;   // 主伤后重置，隔离减疗观测
            (void)heal_amount(&ctx, /*target=*/1, /*amount=*/100);
            const int healed = ctx.getPet(1).hp - 4000;
            CHECK_EQ(healed, 50,
                     "T5160.c 2116：回血 100 被压到 50（实测 " + std::to_string(healed) + "）");
            CHECK_EQ(ctx.getPet(1).hp, 4050,
                     "T5160.c 2116：对手 hp 4000→4050（实测 " +
                         std::to_string(ctx.getPet(1).hp) + "）");
        }
        {   // (b) 对照：不施放 → heal 100 全额
            ScenarioBattle b(make_party(mk_self(kChenMian)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            ctx.getPet(1).hp = 4000;
            (void)heal_amount(&ctx, /*target=*/1, /*amount=*/100);
            const int healed = ctx.getPet(1).hp - 4000;
            CHECK_EQ(healed, 100,
                     "T5160.d 2116 对照：未施放 → heal 100 全额（实测 " +
                         std::to_string(healed) + "）");
        }
        std::cout << "  [T5160] 2116 先手回血压制 + 对照 通过\n";
    }

    std::cout << "scenario_5160 PASS (J194 先手异反/回血压制族)" << std::endl;
    return 0;
}
