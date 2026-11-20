#include <utils/sbs_rand.h>
// 场景 2280 —— J133 先制窗族（3 宏，moves_lib/j_prio_window.cpp）
//（2026-10-04，worktree ../agents/j133；号段 2280-2289 → T2280-T2282 三块）
//
// 断言口径（先制读数法：读 R 桶跑完后的 ws.preemptive_level——对 pri 基值泄漏免疫，
// 比"先手翻盘"断言更精确）：
//   - 1026（束缚之辰 26243 [2,1] 属性技 prio0 纯链）：R1 载体 → R2/R3 对手桶后
//     readout=-1（下 2 回合每回合 -1）、R4=0（窗过期）。
//   - 635（葬身术 19080 [3] 特80 pri=3——全族 pri=3）：R1 对手预置 +2 → 吸收成功 →
//     R2（self 选 pri0 填充刀）readout=+3 恰为窗值；对照 = 对手无提升（吸收失败）
//     → R2 readout=0。
//   - 1203（心灵穿刺击 31362 [300,1,3] 物150 prio0 纯链）：a) atk=1（红伤 1-2<300）
//     → R2 readout=+3、R3=0（1 次消费完）；b) atk=999（红伤>300）→ 计数不落 →
//     R2 readout=0。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0 填充刀
constexpr int kShuFu   = 26243;  // 束缚之辰 [1026]|[3,2]：属性技（1026 载体）
constexpr int kZangShen= 19080;  // 葬身术 [635]|[3]：特80 pri=3（635 载体）
constexpr int kXinLing = 31362;  // 心灵穿刺击 [1203]|[300,1,3]：物150（1203 载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
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

    // ══ T2280 1026 下3回合对手先制-2（束缚之辰）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kShuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 5; ++r) {
            // R1 用载体（开窗 R2-R3），之后拍打填充（避免续窗遮蔽过期断言）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, r == 0 ? 0 : 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int lv = ctx.ws.preemptive_level[1];
            if (r == 1) {
                CHECK_EQ(lv, -1, "T2280 1026：R2 对手先制-1（实测 " + std::to_string(lv) + "）");
            } else if (r == 2) {
                CHECK_EQ(lv, -1, "T2280 1026：R3 仍 -1（实测 " + std::to_string(lv) + "）");
            } else if (r == 3) {
                CHECK_EQ(lv, 0, "T2280 1026：R4 窗过期归零（实测 " + std::to_string(lv) + "）");
            }
        }
        std::cout << "  [T2280] 1026 对手先制削减窗 通过\n";
    }

    // ══ T2281 635 吸收成功 → 下回合先制+1（葬身术 [1]；⚠️ 全族 pri=3 且选择期基值
    //     常驻泄漏——断言用「成功/失败双跑读数差」，对泄漏免疫）══
    {
        // 双跑：a) 对手 +2（吸收成功）vs b) 无提升（吸收失败）——R2/R4 读数差 = 窗贡献
        int r2_success = 0, r2_fail = 0, r4_success = 0, r4_fail = 0;
        for (int run = 0; run < 2; ++run) {
            ScenarioBattle b(make_party(
                                 mk_pet(1, "己方", {kZangShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                             make_party(mk_pet(2, "对手",
                                               {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            if (run == 0) {
                (void)stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2（被吸收对象）
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (run == 0) {
                CHECK(ctx.ability_levels[0][0] == 2,
                      "T2281 前置：吸收归我（self slot0 实测 " +
                          std::to_string(ctx.ability_levels[0][0]) + "）");
            } else {
                CHECK(ctx.ability_levels[0][0] == 0,
                      "T2281 前置：无提升不吸（self slot0 实测 " +
                          std::to_string(ctx.ability_levels[0][0]) + "）");
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 pri0 填充刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            (run == 0 ? r2_success : r2_fail) = ctx.ws.preemptive_level[0];
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 填充刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4 填充刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            (run == 0 ? r4_success : r4_fail) = ctx.ws.preemptive_level[0];
        }
        CHECK_EQ(r2_success - r2_fail, 1,
                 "T2281 635：下回合先制窗 +1（成功 " + std::to_string(r2_success) +
                     " vs 失败 " + std::to_string(r2_fail) + "）");
        CHECK_EQ(r4_success - r4_fail, 0,
                 "T2281 635：R4 窗已过期（差 " +
                     std::to_string(r4_success - r4_fail) + "）");
        std::cout << "  [T2281] 635 吸收先制 通过\n";
    }

    // ══ T2282 1203 低伤 → 下1次技能先制+3（心灵穿刺击）══
    {
        // a) atk=1（红伤 1-2 < 300）→ R2 readout=+3、R3=0
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXinLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 填充刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.preemptive_level[0], 3,
                     "T2282.a 1203：低伤 → 下次技能先制+3（实测 " +
                         std::to_string(ctx.ws.preemptive_level[0]) + "）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 填充刀
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                     "T2282.a 1203：1 次消费完（实测 " +
                         std::to_string(ctx.ws.preemptive_level[0]) + "）");
        }
        // b) atk=999（红伤 > 300）→ 计数不落 → R2 readout=0
        {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kXinLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 999)),
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
                     "T2282.b 1203 对照：高伤不触发（实测 " +
                         std::to_string(ctx.ws.preemptive_level[0]) + "）");
        }
        std::cout << "  [T2282] 1203 低伤计数先制 通过\n";
    }

    std::cout << "scenario_2280 PASS (J133 先制窗族 1026/635/1203)" << std::endl;
    return 0;
}
