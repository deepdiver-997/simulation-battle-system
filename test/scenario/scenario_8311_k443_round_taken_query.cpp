// 场景 8311：K443 · 内核「本回合被攻击实扣累计」查询面最小单测
// —— ws.round_taken_attack_damage[side]（battleWorkspace.h 头注 = 权威口径；
//    写入点 = battleFsm.cpp apply_resolved_damage，攻击技能伤害落地唯一收口；
//    K443 内核 commit 的一部分，先于 929 插件单独验证——号段 8310-8319）
//
// 断言口径（全部**自洽精确等式**，与红伤 85%~100% 浮动无关）：
//   累计值 == 该回合实际净掉血（hp_before - hp_after）——查询面的定义即实扣，
//   浮动只影响两边共同的具体值，不影响等式本身。
//   · T1 落地自洽腿：对手拍打 → 累计 = 净掉血；未被打一侧累计 = 0。
//   · T2 护盾腿：全吸（盾 ≥ 伤害 → 掉血 0 → 累计 0）/ 半吸（1 点盾 → 累计 = 净掉血，
//     吸收的 1 点不入账——实扣裁决；变异「申报值入账」在此转红）。
//   · T3 负测腿（fail-closed，非攻击结算通道不入账）：效果通道直调固伤（粉）/属性
//     直伤 / 回血 —— 累计全部纹丝不动（唯一写者 = 攻击落地收口）。
//   · T4 回合边界腿（同局三回合）：R1 挨刀 → R2 空过（ROUND_START resetWorkspace 已
//     跑）→ 累计 = 0；R3 挨刀 → 累计只含本回合（≠ R1+R3；变异「跨回合残留」转红）。
//   · T5 过量击杀腿：致死一刀钳 0 → 累计 = 掉血前体力（≠ 申报伤害）。
// 断言种子无关（make_pet 清 crit_rate；等式断言不吃浮动）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/skills.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kPaiDa = 10004;  // 拍打：物40 无效果填充刀（crit_rate=0）

// 攻击方（side 1 侧用）：拍打必中。防守方（side 0）：不动吃刀。
ElfPet striker(int atk) {
    ElfPet p = scenario::make_pet(1, "攻击方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, atk, /*hp=*/400000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}
ElfPet tank(int hp = 2000000) {
    ElfPet p = scenario::make_pet(0, "承伤方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                  /*speed=*/100, /*atk=*/1, hp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

// 一回合：我方（side 0）不动，对手（side 1）拍打。
void drive_foe_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// 一回合：双方都不动（纯空过，推回合边界）。
void drive_idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══════════ T1 落地自洽腿：累计 = 净掉血；未被打侧 = 0 ══════════
    {
        ScenarioBattle b(scenario::make_party(tank()), scenario::make_party(striker(20)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;
        drive_foe_attack(b);
        const int delta = hp_before - ctx.getPet(0).hp;
        CHECK(delta > 0, "T1 前提：拍打确实落地（净掉血>0，实际 " + std::to_string(delta) + "）");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], delta,
                 "T1 核心：累计 == 净掉血（实扣口径，浮动无关）");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[1], 0, "T1 未被打一侧累计=0");
        std::cout << "  [T1] 落地自洽腿通过（delta=" << delta << "）\n";
    }

    // ══════════ T2 护盾腿：吸收部分不入账 ══════════
    // T2a 全吸：盾 ≥ 伤害 → 掉血 0 → 累计 0（盾确实被消耗 = 真的全吸了）
    {
        ScenarioBattle b(scenario::make_party(tank()), scenario::make_party(striker(20)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/100000,
                                       /*source_id=*/91001, /*duration=*/0);
        const int hp_before = ctx.getPet(0).hp;
        drive_foe_attack(b);
        const int delta = hp_before - ctx.getPet(0).hp;
        CHECK_EQ(delta, 0, "T2a 前提：盾全吸（本体零掉血）");
        CHECK(ctx.getPet(0).shield_bank_.total() < 100000, "T2a 前提：盾确实被消耗");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], 0,
                 "T2a 核心：护盾吸收不入账（实扣裁决）");
        std::cout << "  [T2a] 护盾全吸腿通过\n";
    }
    // T2b 半吸：1 点盾恰吸 1 → 累计 = 净掉血（申报值入账的变异在此转红：差恰 1）
    {
        ScenarioBattle b(scenario::make_party(tank()), scenario::make_party(striker(20)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).shield_bank_.add(/*priority=*/0, /*quantity=*/1,
                                       /*source_id=*/91002, /*duration=*/0);
        const int hp_before = ctx.getPet(0).hp;
        drive_foe_attack(b);
        const int delta = hp_before - ctx.getPet(0).hp;
        CHECK(delta > 0, "T2b 前提：穿盾落地（净掉血>0）");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], delta,
                 "T2b 核心：累计 == 净掉血（吸收的 1 点已排除）");
        std::cout << "  [T2b] 护盾半吸腿通过（delta=" << delta << "）\n";
    }

    // ══════════ T3 负测腿：效果通道伤害/回血不入账（fail-closed） ══════════
    {
        ScenarioBattle b(scenario::make_party(tank()), scenario::make_party(striker(20)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_foe_attack(b);   // 先让攻击落地造底数
        const int base = ctx.ws.round_taken_attack_damage[0];
        CHECK(base > 0, "T3 前提：攻击落地累计>0");
        const int hp1 = ctx.getPet(0).hp;

        // ① 效果通道固伤（粉伤，直调原语——非攻击结算路径）
        REQUIRE(fixed_damage(&ctx, 0, 500) == FixedDamageResult::SUCCESS);
        CHECK_EQ(ctx.getPet(0).hp, hp1 - 500, "T3a 前提：粉伤确实扣了 500");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], base, "T3a 核心：粉伤不入账");

        // ② 属性直伤（红伤落血但非攻击技能结算——deal_attribute_damage 内部自走）
        const int elems[2] = {0, 0};
        REQUIRE(deal_attribute_damage(&ctx, 0, 300, elems, 1) == FixedDamageResult::SUCCESS);
        CHECK(ctx.getPet(0).hp < hp1 - 500, "T3b 前提：属性直伤确实扣了血");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], base, "T3b 核心：属性直伤不入账");

        // ③ 回血（体力上升不得反向侵蚀累计）
        REQUIRE(heal_amount(&ctx, 0, 1000) == HealResult::SUCCESS);
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], base, "T3c 核心：回血不动累计");
        std::cout << "  [T3] 负测腿（粉/属性/回血不入账）通过\n";
    }

    // ══════════ T4 回合边界腿：R1 挨刀 → R2 空过清零 → R3 只含本回合 ══════════
    {
        ScenarioBattle b(scenario::make_party(tank()), scenario::make_party(striker(20)));
        b.start();
        BattleContext& ctx = b.ctx();
        drive_foe_attack(b);   // R1：挨一刀
        const int r1 = ctx.ws.round_taken_attack_damage[0];
        CHECK(r1 > 0, "T4 前提：R1 累计>0");

        drive_idle_round(b);   // R2：双方不动 → drive 返回时已进 R3 选择期，
                               // 途中 R2 的 ROUND_START → resetWorkspace 已把 ws 清零
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], 0,
                 "T4 核心：回合边界清零（跨入新回合后累计=0，与既有回合窗 reset 口径一致）");

        const int hp_before_r3 = ctx.getPet(0).hp;
        drive_foe_attack(b);   // R3：再挨一刀
        const int delta3 = hp_before_r3 - ctx.getPet(0).hp;
        CHECK(delta3 > 0, "T4 前提：R3 落地");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], delta3,
                 "T4 核心：R3 累计只含本回合（≠ r1+delta3=" + std::to_string(r1 + delta3)
                     + "——变异「跨回合残留」在此转红）");
        std::cout << "  [T4] 回合边界腿通过（r1=" << r1 << " delta3=" << delta3 << "）\n";
    }

    // ══════════ T5 过量击杀腿：钳 0 净掉血 ══════════
    {
        ScenarioBattle b(scenario::make_party(tank(/*hp=*/30000)),
                         scenario::make_party(striker(/*atk=*/100000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(0).hp;
        drive_foe_attack(b);   // 致死一刀
        CHECK_EQ(ctx.getPet(0).hp, 0, "T5 前提：过量击杀（体力归 0）");
        CHECK_EQ(ctx.ws.round_taken_attack_damage[0], hp_before,
                 "T5 核心：累计 == 掉血前体力（钳 0 净掉血，≠ 申报伤害）");
        std::cout << "  [T5] 过量击杀腿通过（hp_before=" << hp_before << "）\n";
    }

    std::cout << "scenario_8311 全部通过\n";
    return 0;
}
