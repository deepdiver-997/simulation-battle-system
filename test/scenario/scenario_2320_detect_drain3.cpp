// 场景 2320 —— J137 DETECT 回吸族（1415 / 687 / 1675，moves_lib/j_detect_drain3.cpp）
//（2026-10-04，worktree ../agents/j137；号段 2320-2329 → T2320-T2322 三块）
//
// 断言口径：
//   - 纯链载体：恶魔汲血刃 33161 [1415]|[50,100]（物160）、熔岩裂地 16791
//     [687]|[2,100]（物80，烧伤门）、寻息追踪 34915 [1675]|[2]（物85）；
//   - 1415/687 红伤 final 用 ws.raw_attack_damage[0] 精确差账（heal=final×pct/100
//     同款整除）；1415 低体档直写 hp=6000（max 99999 不触钳）、非低体档对手压到
//     5000（6000<5000 不成立 → 50% 档）；687 烧伤 setup 用 try_apply_anomaly；
//   - 1675 反转门：setup 对手 +2 攻 → 反转回 0 + HEAL_BLOCK 探针（heal_amount 直调
//     被钳 = 封回血生效）；R3 窗外同探针恢复（[2] 票=绝对窗 R0..R1）；无提升对照
//     反转失败 → 不封。
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

constexpr int kEMo    = 33161;   // 恶魔汲血刃：物160 [1415] → [50,100]
constexpr int kRongYan= 16791;   // 熔岩裂地：物80 [687] → [2,100]（烧伤门 100%）
constexpr int kXunXi  = 34915;   // 寻息追踪：物85 [1675] → [2]
constexpr int kPaDa   = 10004;   // 拍打：filler

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int atk = 1000) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, atk, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, hp);
    always_hit(p);
    return p;
}

int raw0(BattleContext& ctx) { return ctx.ws.raw_attack_damage[0]; }

void both_pass(ScenarioBattle& b) {
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

    // ══ T2320 1415 回吸：低体 100% / 非低体 50%（恶魔汲血刃 [50,100]）══
    {
        {   // (a) 低体（6000 < 99999）→ 提升至 100%：me = 6000 + final
            ScenarioBattle b(make_party(self_pet({kEMo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 6000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int final = raw0(ctx);
            CHECK(final > 0, "T2320.a 前置：红伤落地");
            CHECK_EQ(ctx.getPet(0).hp, 6000 + final,
                     "T2320.a 1415：低体 → 100% 回吸（heal=final）");
        }
        {   // (b) 非低体（6000 < 5000 不成立）→ 基础 50%：me = 6000 + final/2
            ScenarioBattle b(make_party(self_pet({kEMo, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                /*hp=*/5000)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 6000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int final = raw0(ctx);
            CHECK(final > 0 && final < 5000, "T2320.b 前置：红伤落地且对手存活");
            CHECK_EQ(ctx.getPet(0).hp, 6000 + final / 2,
                     "T2320.b 1415：非低体 → 50% 回吸（整除）");
        }
        std::cout << "  [T2320] 1415 低体提升回吸 通过\n";
    }

    // ══ T2321 687 烧伤门 100% 回吸（熔岩裂地 [2,100]）══
    {
        {   // (a) 对手烧伤 → heal = final
            ScenarioBattle b(make_party(self_pet({kRongYan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 5000;
            REQUIRE(try_apply_anomaly(&ctx, 1, kBurn, /*duration=*/3, /*actor=*/0));
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int final = raw0(ctx);
            CHECK(final > 0, "T2321.a 前置：红伤落地");
            CHECK_EQ(ctx.getPet(0).hp, 5000 + final, "T2321.a 687：对手烧伤 → 100% 回吸");
        }
        {   // (b) 无烧伤 → 不回
            ScenarioBattle b(make_party(self_pet({kRongYan, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 5000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(raw0(ctx) > 0, "T2321.b 前置：红伤落地");
            CHECK_EQ(ctx.getPet(0).hp, 5000, "T2321.b 687：对手无烧伤 → 不回血");
        }
        std::cout << "  [T2321] 687 异常门回吸 通过\n";
    }

    // ══ T2322 1675 反转成功 → HEAL_BLOCK [2]；无提升反转失败不封（寻息追踪）══
    {
        {   // (a) 对手 +2 攻 → 反转回 0 + 封回血两回合（R1/R2 封、R3 解）
            ScenarioBattle b(make_party(self_pet({kXunXi, kPaDa, kPaDa, kPaDa, kPaDa},
                                                 /*atk=*/1)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, 2) == StatChangeResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -2, "T2322.a 1675：反转成功 → 对手攻 +2 变号 -2");
            // 为封疗与解封探针留足缺口，避免满血钳制造成假绿或假红。
            ctx.getPet(1).hp = 50000;
            const int hp_before = ctx.getPet(1).hp;
            (void)heal_amount(&ctx, 1, 100);
            CHECK_EQ(ctx.getPet(1).hp, hp_before, "T2322.a 1675：HEAL_BLOCK 生效（R1 治疗被钳）");
            // 票=绝对窗 register_round=0、duration=2 → 覆盖 roundCount {0,1}（R1、R2
            // 两个驱动回合；R2 驱动完成后 roundCount=2 已过期——窗口判据
            // window_expired: current-register >= duration）。
            CHECK(ctx.is_immune(1, ImmunityType::HEAL_BLOCK, ctx.currentState),
                  "T2322.a 1675：R1 末票仍在窗内");
            both_pass(b);   // R2（窗内最后回合）
            both_pass(b);   // R3 窗外 → 解封
            (void)heal_amount(&ctx, 1, 100);
            CHECK_EQ(ctx.getPet(1).hp, hp_before + 100, "T2322.a 1675：R3 窗外解封");
        }
        {   // (b) 无提升 → 反转失败 → 不封
            ScenarioBattle b(make_party(self_pet({kXunXi, kPaDa, kPaDa, kPaDa, kPaDa},
                                                 /*atk=*/1)),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            ctx.seerRobot[1].elfPets[0].hp = 50000;   // 造治疗头寸
            const int hp_before = ctx.getPet(1).hp;
            (void)heal_amount(&ctx, 1, 500);
            CHECK_EQ(ctx.getPet(1).hp, hp_before + 500,
                     "T2322.b 1675：反转失败 → 不封回血");
        }
        std::cout << "  [T2322] 1675 反转封回血 通过\n";
    }

    std::cout << "scenario_2320 PASS (J137 DETECT 回吸族)" << std::endl;
    return 0;
}
