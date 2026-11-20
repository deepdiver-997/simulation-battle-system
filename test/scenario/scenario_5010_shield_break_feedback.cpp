// 场景 5010 —— J186 护盾/护罩破反馈族二（1422 / 1661，moves_lib/j_shield_break_feedback.cpp）
//（2026-10-22，worktree ../agents/j186；号段 5010-5019）
//
// 断言口径：
//   - 1422（元宵甜甜 27074 链 [48,1023,1422]|[4,3,100,3,300,300]）：
//     R1 获护罩 300 → 直调粉伤 400（护罩吃 300、本体吃 100）→ 破罩回 300。
//     起始 hp 故意压到 4000（max 5000），回血不被 clamp：期望 4000-100+300=4200。
//     对照 = 不施放载体（无护罩）→ 同样粉伤 400 → hp 3600、回 0。
//   - 1661（耀世光辉 27731 链 [191,1661,1474]|[4,400,100,7,3,80]）：
//     R1 获护盾 400 → 对手物攻破盾 → 100% 令对手疲惫(7)。断言盾归零 + 对手异常 7。
//     对照 = 不施放载体 → 对手攻击打本体，对手无异常。
//   - 事件是"入队 + 晚 drain"：直调 deal_damage 后手动 drain 一次（与 FSM 内 drain 同语义）。
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

constexpr int kPaiDa       = 10004;  // 拍打：物 40（对手填充刀）
constexpr int kYuanXiao     = 27074;  // [48,1023,1422]|[4,3,100,3,300,300]（护罩 300/回血 300）
constexpr int kYaoShi       = 27731;  // [191,1661,1474]|[4,400,100,7,3,80]（护盾 400/100%疲惫）
constexpr int kFatigue      = 7;      // 疲惫（AbnormalStatusId::Fatigue）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5010 1422 护罩破 → 回 {1} 点体力 ══
    {
        {   // (a) 获护罩 300 → 粉伤 400（罩吃 300、本体 100）→ 破罩回 300
            ScenarioBattle b(
                make_party(self_pet({kYuanXiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;   // 压低起始体力，避免回血 clamp 掩盖
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 300,
                     "T5010.a 前置：1422 获护罩 300（实测 " +
                         std::to_string(ctx.getPet(0).hood_bank_.total()) + "）");
            deal_damage(&ctx, 0, 400, DamageKind::FIXED, /*actor=*/1);
            ctx.event_center_.drain(&ctx, ctx.roundCount);
            CHECK_EQ(ctx.getPet(0).hood_bank_.total(), 0,
                     "T5010.a 1422：粉伤 400 击破护罩（剩余 " +
                         std::to_string(ctx.getPet(0).hood_bank_.total()) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 4200,
                     "T5010.a 1422：破罩回 300（4000-100+300，实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        {   // (b) 对照：不施放载体 → 无护罩 → 粉伤 400 全落本体、无回血
            ScenarioBattle b(
                make_party(self_pet({kYuanXiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            deal_damage(&ctx, 0, 400, DamageKind::FIXED, /*actor=*/1);
            ctx.event_center_.drain(&ctx, ctx.roundCount);
            CHECK_EQ(ctx.getPet(0).hp, 3600,
                     "T5010.b 1422 对照：无护罩不回血（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T5010] 1422 护罩破回血 + 对照 通过\n";
    }

    // ══ T5010 1661 护盾破 → 概率异常对手 ══
    {
        {   // (a) 获护盾 400 → 对手物攻破盾 → 100% 疲惫
            ScenarioBattle b(
                make_party(self_pet({kYaoShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/3000, /*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 获盾
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 破盾
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T5010.c 1661：护盾被击破（剩余 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
            CHECK(ctx.has_active_abnormal_status(1, kFatigue),
                  "T5010.c 1661：破盾 100% 令对手疲惫");
        }
        {   // (b) 对照：不施放载体 → 对手攻击不破盾（无盾）→ 对手无异常
            ScenarioBattle b(
                make_party(self_pet({kYaoShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100, /*atk=*/3000, /*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFatigue),
                  "T5010.d 1661 对照：未获盾 → 对手无疲惫");
        }
        std::cout << "  [T5010] 1661 护盾破异常 + 对照 通过\n";
    }

    std::cout << "scenario_5010 PASS (J186 护盾/护罩破反馈族二)" << std::endl;
    return 0;
}
