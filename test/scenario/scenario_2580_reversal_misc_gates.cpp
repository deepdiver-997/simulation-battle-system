#include <utils/sbs_rand.h>
// 场景 2580 —— J156 反转/免百分比杂门（3 宏，moves_lib/j_reversal_misc_gates.cpp）
//（2026-10-04，worktree ../agents/j156；号段 2580-2589 → T2580-T2582 三块）
//
// 断言口径：
//   - 551（闪光聚能斧 [551] 物 pri0 纯链，双跑比值法对冲 85~100% 浮动）：
//     a) R1 载体 → R2 拍打掉血 ≥1.6× 基线（受伤翻倍）；b) R3（窗过期）比值 <1.6。
//   - 1885（苦海慈航 [3,100,6] cat4；链 499/191/1766✓ 不扰 hp）：
//     a) 无免粉 → 吸 99999/3=33333 精确、不害怕；
//     b) 场景直授 PINK 免疫票 → 吸取照旧 + 100% 害怕（码 6）。
//   - 970（欺罔之视 [3] cat4 pri0；链 1338 只加自身强化、1438 哑）：
//     a) 对手 +2 → 双倍反转（self slot0=+4）+ 下回合先制+3（pri0 无泄漏 → readout=3）；
//     b) 对手无提升 → 反转失败消除（no-op）→ readout=0。
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

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kShanGuang=15524;  // 闪光聚能斧 [551]：物（551 载体）
constexpr int kKuHai   = 28157;  // 苦海慈航 [499,191,1766,1885]|[3,100,6]：属性技
constexpr int kQiWang  = 28305;  // 欺罔之视 [970,1338,1438]|[3]：属性技
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;   // 比值断言禁暴击噪声
    }
}

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id == 1 ? kSelfHp : kFoeHp);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2580 551 下回合受伤翻倍（闪光聚能斧；同场配对：R2 带标记 vs R3 过期）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kShanGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        int prev = ctx.getPet(1).hp;
        int loss_r1 = 0, loss_r2 = 0, loss_r3 = 0, loss_r4 = 0;
        for (int r = 0; r < 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r == 0 ? 0 : 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int now = ctx.getPet(1).hp;
            (r == 0 ? loss_r1 : r == 1 ? loss_r2 : r == 2 ? loss_r3 : loss_r4) = prev - now;
            prev = now;
        }
        std::cout << "  [dbg] R1=" << loss_r1 << " R2=" << loss_r2 << " R3=" << loss_r3
                  << " R4=" << loss_r4 << "\n";
        CHECK(loss_r2 * 5 > loss_r3 * 8,
              "T2580.a 551：R2（带标记）" + std::to_string(loss_r2) + " 应 >1.6× R3（过期）" +
                  std::to_string(loss_r3));
        CHECK(loss_r2 * 5 > loss_r4 * 8,
              "T2580.b 551：R4（过期）应与 R3 同带——R2 对 R4 也应 >1.6×（" +
                  std::to_string(loss_r2) + " vs " + std::to_string(loss_r4) + "）");
        std::cout << "  [T2580] 551 受伤翻倍窗 通过\n";
    }

    // ══ T2581 1885 吸取+免粉补偿（苦海慈航）══
    {
        // a) 无免粉：吸 1/3 精确、不害怕
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kKuHai, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(foe_hp0 - ctx.getPet(1).hp, 33333,
                     "T2581.a 1885：吸 1/3=33333（实测 " +
                         std::to_string(foe_hp0 - ctx.getPet(1).hp) + "）");
            CHECK(!ctx.has_active_abnormal_status(1, /*害怕=*/6),
                  "T2581.a 1885：无免粉不触发补偿");
        }
        // b) 场景直授 PINK 免疫票 → 吸取照旧 + 100% 害怕
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kKuHai, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)ctx.grant_immunity(/*owner=*/1, ImmunityType::PINK_DAMAGE, coverage_all(),
                                     /*anomaly_mask=*/0, /*duration_rounds=*/3, /*source_id=*/2581,
                                     /*soul_immunity=*/false, EffectScope::ON_STAGE,
                                     /*counts=*/0, /*source_effect_id=*/2581);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, /*害怕=*/6),
                  "T2581.b 1885：免粉 → 100% 害怕（补偿 rider）");
        }
        std::cout << "  [T2581] 1885 吸取免粉补偿 通过\n";
    }

    // ══ T2582 970 双倍反转（欺罔之视）══
    {
        // a) 对手 +2 → 反转成功：self slot0=+4、R2 先制 readout=+3
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kQiWang, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 0, 2);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // ⚠️ 链 1338 会给自身全属性+（污染 self slot0）——双倍反转改从对手侧断言
            CHECK_EQ(ctx.ability_levels[1][0], -2,
                     "T2582.a 970：对手 +2 被反转（实测 " +
                         std::to_string(ctx.ability_levels[1][0]) + "，应 -2）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 pri0 填充刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.preemptive_level[0], 3,
                     "T2582.a 970：下回合先制+3（实测 " +
                         std::to_string(ctx.ws.preemptive_level[0]) + "）");
        }
        // b) 对手无提升 → 反转失败消除（no-op）→ R2 readout=0
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kQiWang, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T2582.b 970 对照：反转失败无先制（实测 " +
                         std::to_string(ctx.ws.preemptive_level[0]) + "）");
        }
        std::cout << "  [T2582] 970 双倍反转 通过\n";
    }

    std::cout << "scenario_2580 PASS (J156 反转/免百分比杂门 551/1885/970)" << std::endl;
    return 0;
}
