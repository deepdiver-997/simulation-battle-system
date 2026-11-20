// 场景 5330 —— J207 吞噬强化双倍转移族（569，moves_lib/j_devour_boosts.cpp）
//（2026-10-22，号段 5330-5329）
//
// 断言口径：
//   - 569（狂魔霸裂斩 15444 纯链，物 160）：
//     · (a) 对手攻+2/速+1 → 转移后对手两槽清零，自身攻 +4、速 +2（双倍）；
//       审计 990569100=3（转移总量）/990569101=3（补加总量）。
//     · (b) 对照：对手无正等级 → 自身全槽不变、审计 0。
//     · (c) +6 钳制：自身攻已 +5、对手攻+2 → 转移 +1（钳到 +6），补加意图 1 但已到顶；
//       审计 moved=2 / doubled=1，最终自身攻=6。
//   - 断言读精确槽位，不用"等级和"。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;
constexpr int kKuangMo = 15444;  // [569]
constexpr int kMovedKey = 990569100;
constexpr int kDoubledKey = 990569101;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kKuangMo, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

int storage_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

void use_carrier(ScenarioBattle& b) {
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

    // ══ T5330 569 吞噬强化并双倍转移 ══
    {
        {   // (a) 对手 +2 攻 / +1 速 → 自身 +4 攻 / +2 速
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, /*target=*/1, /*stat=*/0, /*delta=*/2);
            (void)stat_change(&ctx, /*target=*/1, /*stat=*/4, /*delta=*/1);
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[1][0], 0, "T5330.a 569：对手攻槽被吞噬");
            CHECK_EQ(ctx.ability_levels[1][4], 0, "T5330.a 569：对手速槽被吞噬");
            CHECK_EQ(ctx.ability_levels[0][0], 4, "T5330.a 569：自身攻 +4（双倍 2）");
            CHECK_EQ(ctx.ability_levels[0][4], 2, "T5330.a 569：自身速 +2（双倍 1）");
            CHECK_EQ(storage_int(ctx, kMovedKey), 3, "T5330.a 569：转移总量 3");
            CHECK_EQ(storage_int(ctx, kDoubledKey), 3, "T5330.a 569：补加总量 3");
        }
        {   // (b) 对照：无提升态
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[1][0], 0, "T5330.b 对照：对手攻槽 0");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T5330.b 对照：自身攻槽不变");
            CHECK_EQ(storage_int(ctx, kMovedKey), 0, "T5330.b 对照：转移总量 0");
            CHECK_EQ(storage_int(ctx, kDoubledKey), 0, "T5330.b 对照：补加 0");
        }
        {   // (c) +6 钳制
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, /*target=*/0, /*stat=*/0, /*delta=*/5);
            (void)stat_change(&ctx, /*target=*/1, /*stat=*/0, /*delta=*/2);
            use_carrier(b);
            CHECK_EQ(ctx.ability_levels[1][0], 0, "T5330.c 569：对手攻槽清零");
            CHECK_EQ(ctx.ability_levels[0][0], 6,
                     "T5330.c 569：自身攻钳在 +6（实测 " +
                         std::to_string(ctx.ability_levels[0][0]) + "）");
            CHECK_EQ(storage_int(ctx, kMovedKey), 2, "T5330.c 569：转移总量 2");
            CHECK_EQ(storage_int(ctx, kDoubledKey), 1, "T5330.c 569：实际搬入 1 点");
        }
        std::cout << "  [T5330] 569 吞噬强化双倍转移 + 对照 + +6 钳制 通过\n";
    }

    std::cout << "scenario_5330 PASS (J207 吞噬强化双倍转移族)" << std::endl;
    return 0;
}
