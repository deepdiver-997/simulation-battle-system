#include <utils/sbs_rand.h>
// 场景 061：通用特性批次 C —— 顽强（Eid 31/147）/ 回神（Eid 33/148）致死存活。
//
// 口径来源：赛学必修6《精灵特性》+ 用户 2026-09-16 拍板：
//   - 顽强："受到**致死攻击**时有 n% 几率余下 m 点体力（0 体力也可以触发）"；
//     实际概率 3/3.5/4/5/6/7%（DB args 3/35/40/50/60/70）、残留点数 1/1/1/1/2/2（args[1]）。
//   - 回神："体力降低到 **1/8** 时有 n% 几率体力回满（0 体力也可以触发）"；
//     DB args = {8, 概率}（分母在前、概率在后）。
//   - ①**仅战斗阶段**触发（"若是受到回合结束后的致死伤害，则会直接被击杀"）；
//     ②"属于**强制残留体力**，不受削续航影响"；
//   - ★ 与归零原语的关系（用户拍板）：**不加短路**——"归零与强制保留/回满并不冲突，
//     看先后顺序定实际效果"；实现上钩位在 `trait_instant_kill_zero_hook` **之后**，
//     即"先归零、后判定存活"。
//
// 验证点：
//   ⚠️ 测试构造：本族概率编码是"<10 按整百分点、≥10 按千分点"（真实数据最多 70‰=7%），
//      没有"100%"档 → 测试传 **1000‰**（≥1000 走 trait_proc_roll 的短路必发、不消耗 rand）。
//   S1 顽强（必发）：被秒杀级伤害打死 → 余下 1 点体力
//   S2 ★ 顽强 vs 瞬杀归零：对手带 100% 瞬杀（走 force_hp_to_zero 归零）→ 顽强在归零**之后**
//      把 0 体力抬回 1 点（"先归零、后判定，不短路"的实证）
//   S3 回神：体力被打到 ≤ 1/8 → 回满（100% 档）
//   S4 回神阈值不是致死也触发：伤害只需把体力压到 1/8 以下（不必致死）
//   S5 顽强不触发的情形：未被致死（体力仍 >0）→ 不抬血
//   S6 回合结束的致死伤害不触发（仅战斗阶段）：用 hook 之外的路径验证——
//      直接调 force_hp_to_zero（非攻击伤害出口）→ 顽强不介入
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <effects/trait_state.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物攻 40
using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

ElfPet strong_attacker(CommonTrait trait = {}) {
    // 攻方高攻 + 高体力，保证能一击打死守方
    return scenario::make_pet(0, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              200, 4000, 4000, {}, std::move(trait));
}
ElfPet frail_defender(CommonTrait trait, int hp = 50) {
    return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              100, 10, hp, {}, std::move(trait));
}

void strike(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== S1 顽强：致死后余下 1 点 ==========
    {
        ScenarioBattle b = make_battle(strong_attacker(),
                                       frail_defender(scenario::make_trait("顽强", 5, {1000, 1})));
        b.start();
        strike(b);
        CHECK_EQ(b.pet(1).hp, 1, "S1 顽强应余下 1 点体力");
        std::cout << "S1 PASS 顽强：致死后余下 1 点\n";
    }

    //========== S1b 顽强 5 星：残留 2 点（args[1]=2）==========
    {
        ScenarioBattle b = make_battle(strong_attacker(),
                                       frail_defender(scenario::make_trait("顽强", 5, {1000, 2})));
        b.start();
        strike(b);
        CHECK_EQ(b.pet(1).hp, 2, "S1b 顽强 5 星应余下 2 点（args[1]=2）");
        std::cout << "S1b PASS 顽强 5 星：余下 2 点\n";
    }

    //========== S2 ★ 顽强 vs 瞬杀归零：归零之后抬回（不短路）==========
    {
        // 攻方带 100% 瞬杀（args[0]=1000 千分点 = 100%）→ 红伤后走 force_hp_to_zero 归零；
        // 守方带顽强 → 钩位在归零之后，把 0 体力抬回 1 点。
        ScenarioBattle b = make_battle(
            strong_attacker(scenario::make_trait("瞬杀", 5, {1000, 0})),
            frail_defender(scenario::make_trait("顽强", 5, {1000, 1})));
        b.start();
        strike(b);
        CHECK_EQ(b.pet(1).hp, 1,
                 "S2 瞬杀归零后顽强应抬回 1 点（顺序：先归零、后存活判定）");
        std::cout << "S2 PASS 顽强在瞬杀归零之后生效（不短路）\n";
    }

    //========== S3 回神：致死时回满（0 体力也可以触发）==========
    {
        ScenarioBattle b = make_battle(strong_attacker(),
                                       frail_defender(scenario::make_trait("回神", 5, {8, 1000})));
        b.start();
        const int max_hp = b.pet(1).numericalBase[NumericalPropertyIndex::HP];
        strike(b);
        CHECK_EQ(b.pet(1).hp, max_hp, "S3 回神应在致死后回满");
        std::cout << "S3 PASS 回神：致死回满（" << max_hp << "）\n";
    }

    //========== S4 回神非致死也触发：体力被压到 1/8 以下（未致死）==========
    {
        // 守方 400 体力、阈值 1/8 = 50：把体力预置到 55，用**弱攻**打到阈值以下但不致死。
        // 先跑一场**无特性对照**测出实际伤害，确认"非致死"（否则本组退化成 S3 的致死档）。
        auto make_def = [](CommonTrait trait) {
            return scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                      100, 10, 400, {}, std::move(trait));
        };
        ElfPet weak_atk = scenario::make_pet(0, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             200, 30, 4000);
        ScenarioBattle ctrl = make_battle(weak_atk, make_def({}));
        ctrl.start();
        ctrl.pet(1).hp = 55;
        strike(ctrl);
        const int dmg = 55 - ctrl.pet(1).hp;
        CHECK(dmg > 0 && dmg < 55, "S4 前提：本次弱攻非致死（实伤 " + std::to_string(dmg) + "）");
        CHECK(ctrl.pet(1).hp <= 50, "S4 前提：对照组的体力已落到阈值(50)以下");

        ScenarioBattle b = make_battle(weak_atk, make_def(scenario::make_trait("回神", 5, {8, 1000})));
        b.start();
        b.pet(1).hp = 55;   // 同起点
        strike(b);
        const int max_hp = b.pet(1).numericalBase[NumericalPropertyIndex::HP];
        CHECK_EQ(b.pet(1).hp, max_hp, "S4 回神在非致死的 1/8 阈值也回满");
        std::cout << "S4 PASS 回神在 1/8 阈值触发（非致死，实伤 " << dmg << "）\n";
    }

    //========== S5 顽强不触发：未致死（体力仍 >0）==========
    {
        ElfPet def = scenario::make_pet(1, "守方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                        100, 10, 400, {},
                                        scenario::make_trait("顽强", 5, {1000, 1}));
        ElfPet atk = scenario::make_pet(0, "攻方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                        200, 200, 4000);
        ScenarioBattle b = make_battle(atk, def);
        b.start();
        strike(b);
        CHECK(b.pet(1).hp > 1, "S5 未致死时顽强不应介入（保留实伤后的体力）");
        std::cout << "S5 PASS 顽强需致死才触发（实伤后体力 " << b.pet(1).hp << "）\n";
    }

    //========== S6 非攻击伤害出口的归零不触发顽强（仅战斗阶段）==========
    {
        ScenarioBattle b = make_battle(strong_attacker(),
                                       frail_defender(scenario::make_trait("顽强", 5, {1000, 1})));
        b.start();
        // 直接调归零原语（模拟回合结束类的致死；不走攻击伤害出口）
        const int hp_before = b.pet(1).hp;
        CHECK(hp_before > 0, "S6 前提：守方存活");
        (void)force_hp_to_zero(&b.ctx(), 1, 0);
        CHECK_EQ(b.pet(1).hp, 0, "S6 攻击伤害出口之外的归零不应被顽强救回");
        std::cout << "S6 PASS 仅战斗阶段（攻击出口）触发\n";
    }

    std::cout << "\n场景 061 全部通过：特性批次 C（顽强/回神，归零后存活判定）✓" << std::endl;
    return 0;
}
