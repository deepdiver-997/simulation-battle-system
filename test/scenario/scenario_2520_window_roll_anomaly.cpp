#include <utils/sbs_rand.h>
// 场景 2520 —— J161 窗内掷骰异常族（1007 / 771，moves_lib/j_window_roll_anomaly.cpp）
//（2026-10-04，worktree ../agents/j161；号段 2520-2529 → T2520-T2522 三块）
//
// 断言口径：
//   - 1007 走 泰坦威慑 25993 [3,50,6,1]（属性技武装载体，伴生 843=恢复良性）：
//     50% 掷点用种子校准（序列=[R1 施放零耗, R2 我拍打浮伤, R2 掷点]）——
//     掷中 → 对手中码 6 且无降槽；掷歪 → 无异常且六槽各 -1（level_sum=-6）；
//   - 771 走纯链 石之无畏 28482 [3,100]（100% 短路不耗 rand）：R2 我攻击 →
//     对手 0..9 中恰 1 种激活（计数断言免校准）。
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

constexpr int kTaiTan = 25993;   // 泰坦威慑：属 [1007] → [3,50,6,1]
constexpr int kShiZhi = 28482;   // 石之无畏：属 纯链 [771] → [3,100]
constexpr int kPaDa   = 10004;   // 拍打：我方攻击载体

constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);   // 6

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

void self_skill(ScenarioBattle& b, int slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, slot);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

int foe_anomaly_count(BattleContext& ctx) {
    int n = 0;
    for (int code = 0; code <= 9; ++code) {
        if (ctx.has_active_abnormal_status(1, code)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2520 1007 掷点两半（泰坦威慑 [3,50,6,1]）══
    {
        int seed_hit = -1;
        int seed_miss = -1;
        for (int s = 1; s < 100000 && (seed_hit < 0 || seed_miss < 0); ++s) {
            sbs_srand(s);
            (void)sbs_rand();                 // d1 = R2 我拍打浮伤
            const int d2 = sbs_rand() % 100;  // d2 = R2 申报掷点
            if (seed_hit < 0 && d2 < 50) {
                seed_hit = s;
            }
            if (seed_miss < 0 && d2 >= 50) {
                seed_miss = s;
            }
        }
        REQUIRE(seed_hit > 0);
        REQUIRE(seed_miss > 0);
        {   // (a) 掷中：对手中码 6、无降槽
            ScenarioBattle b(make_party(self_pet({kTaiTan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(seed_hit);
            self_skill(b, 0);   // R1 武装（属性技零 rand 消耗）
            both_act(b, 1);     // R2 我攻击 → 申报掷中
            CHECK(ctx.has_active_abnormal_status(1, kFear), "T2520.a 1007：掷中 → 对手中码 6");
            CHECK_EQ(level_sum(ctx, 1), 0, "T2520.a 1007：掷中 → 无降槽");
        }
        {   // (b) 掷歪：无异常、六槽各 -1
            ScenarioBattle b(make_party(self_pet({kTaiTan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            sbs_srand(seed_miss);
            self_skill(b, 0);
            both_act(b, 1);
            CHECK(!ctx.has_active_abnormal_status(1, kFear), "T2520.b 1007：掷歪 → 无异常");
            CHECK_EQ(level_sum(ctx, 1), -6, "T2520.b 1007：掷歪 → 对手全属性 -1（六槽）");
        }
        std::cout << "  [T2520] 1007 掷点两半 通过 (seed " << seed_hit << "/" << seed_miss
                  << ")\n";
    }

    // ══ T2521 771 随机异常（石之无畏 [3,100]：100% 短路不耗 rand）══
    {
        ScenarioBattle b(make_party(self_pet({kShiZhi, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 武装
        both_act(b, 1);     // R2 我攻击 → 随机 1 种
        CHECK_EQ(foe_anomaly_count(ctx), 1, "T2521 771：随机附加恰 1 种异常（0..9）");
        std::cout << "  [T2521] 771 随机异常 通过\n";
    }

    // ══ T2522 1007 属性技回合不触发（门槛判别：武装回合自身属性技不出手攻击）══
    {
        ScenarioBattle b(make_party(self_pet({kTaiTan, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b, 0);   // R1 我用属性技武装（非攻击技 → tick 门闭，无掷点无降槽）
        CHECK_EQ(level_sum(ctx, 1), 0, "T2522 1007：属性技回合不触发（非攻击技门）");
        std::cout << "  [T2522] 1007 属性技门 通过\n";
    }

    std::cout << "scenario_2520 PASS (J161 窗内掷骰异常族)" << std::endl;
    return 0;
}
