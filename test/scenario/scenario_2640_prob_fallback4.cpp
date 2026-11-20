#include <utils/sbs_rand.h>
// 场景 2640 —— J165 申报兜底族二（2 宏，moves_lib/j_prob_fallback4.cpp）
//（2026-10-04，worktree ../agents/j165；号段 2640-2649 → T2640-T2641 两块）
//
// 断言口径（crit=0；atk=1 红伤 1-2）：
//   - 1000（遮魂裂 32975 [50,15,200] 物 pri0 纯链，p=0.5）：100 局各 1 回合——
//     每局结果互斥二选一：对手中冻伤(15) 或 掉血=200+红伤带 [201,203]；
//     兜底局数 ∈ [35,65]（B(100,0.5) 双侧尾 ~0.3%）。
//   - 1201（乘风化龙 32137 [70,15,2,1] 特 pri0 纯链，p=0.7 先手门）：
//     a) 100 局（self 先手）：R2 先制 readout==+1 当且仅当未中冻伤（互斥完备）；
//        窗局数 ∈ [18,45]（B(100,0.3) 尾 ~1%）；
//     b) 后手门负例（self speed 100）：20 局全不触发（无异常且 R2 readout=0）。
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
constexpr int kZheHun  = 32975;  // 遮魂裂 [1000]|[50,15,200]：物（1000 载体）
constexpr int kChengFeng=32137;  // 乘风化龙 [1201]|[70,15,2,1]：特（1201 载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

ElfPet mk_pet(int id, const char* name, std::array<int, 5> skills, int speed, int atk) {
    ElfPet p = make_pet(id, name, skills, speed, atk, id == 1 ? kSelfHp : kFoeHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261004);

    // ══ T2640 1000 概率异常/未触发固伤（遮魂裂，p=0.5 统计）══
    {
        constexpr int kTrials = 100;
        int fallback = 0;
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kZheHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            const int foe_hp0 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const bool landed = ctx.has_active_abnormal_status(1, /*冻伤=*/15);
            const int loss = foe_hp0 - ctx.getPet(1).hp;
            if (landed) {
                CHECK(loss > 0 && loss < 200,
                      "T2640 异常局：红伤带内（实测 " + std::to_string(loss) + "）");
            } else {
                ++fallback;
                CHECK(loss >= 201 && loss <= 203,
                      "T2640 兜底局：固伤 200+红伤带（实测 " + std::to_string(loss) + "）");
            }
        }
        CHECK(fallback >= 35 && fallback <= 65,
              "T2640 1000 兜底带：100 局 " + std::to_string(fallback) +
                  " 应落 B(100,0.5) [35,65]");
        std::cout << "  [T2640] 1000 统计侧证 通过（兜底 " << fallback << "/" << kTrials
                  << "）\n";
    }

    // ══ T2641 1201 先手概率异常/未触发先制窗（乘风化龙，p=0.7）══
    {
        // a) self 先手：窗局（readout==+1）⟺ 未中冻伤；窗数带
        {
            constexpr int kTrials = 100;
            int window = 0;
            for (int t = 0; t < kTrials; ++t) {
                ScenarioBattle b(make_party(
                                     mk_pet(1, "己方",
                                            {kChengFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                                 make_party(mk_pet(2, "对手",
                                                   {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                const bool landed = ctx.has_active_abnormal_status(1, /*冻伤=*/15);
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 pri0 填充刀
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                const bool prio = ctx.ws.preemptive_level[0] == 1;
                CHECK(landed != prio,
                      "T2641.a 1201：互斥完备（异常局不得有先制窗，实测 landed=" +
                          std::to_string(landed) + " prio=" + std::to_string(prio) + "）");
                if (prio) {
                    ++window;
                }
            }
            CHECK(window >= 18 && window <= 45,
                  "T2641.a 1201 窗带：100 局 " + std::to_string(window) +
                      " 应落 B(100,0.3) [18,45]");
            std::cout << "  [T2641.a] 1201 先手统计 通过（窗 " << window << "/" << kTrials
                      << "）\n";
        }
        // b) 后手门负例：全不触发
        {
            for (int t = 0; t < 20; ++t) {
                ScenarioBattle b(make_party(
                                     mk_pet(1, "己方",
                                            {kChengFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)),
                                 make_party(mk_pet(2, "对手",
                                                   {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)));
                b.start();
                BattleContext& ctx = b.ctx();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
                b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
                b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
                b.drive();
                CHECK(!ctx.has_active_abnormal_status(1, /*冻伤=*/15)
                          && ctx.ws.preemptive_level[0] == 0,
                      "T2641.b 1201 后手门：全不触发（局 " + std::to_string(t) + "）");
            }
            std::cout << "  [T2641.b] 1201 后手门负例 通过\n";
        }
    }

    std::cout << "scenario_2640 PASS (J165 申报兜底族二 1000/1201)" << std::endl;
    return 0;
}
