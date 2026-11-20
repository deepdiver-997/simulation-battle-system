#include <utils/sbs_rand.h>
// 场景 7020 —— J251 防御同调族（45，moves_lib/j_defense_sync.cpp）
//（2026-10-24，worktree ../agents/j251；号段 7020-7029 → T7020-T7021 两块）
//
// 断言口径（crit=0 全程去噪；伤害带按 0.84×atk/def×40+2 推导，×[0.85,1.0] 浮动）：
//   - 45（模仿术 20029 [5] / 真实残影 20336 [3]，全 cat4 pri0 纯链）：
//     self 本体 def=400、对手 atk=300 → R1 用载体后，对手拍打伤害 ≈ self def=100
//     原生量级（与 def=100 对照场比值 ∈[0.8,1.25]）；窗过期后（[5]→R7、[3]→R5）
//     伤害回落 def=400 量级（< 窗内一半）。
//     拍打基准：def=100 → ~102×rand ∈[87,102]；def=400 → ~27×rand ∈[23,27]。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物40 pri0
constexpr int kMoFang  = 20029;  // 模仿术 [45]|[5]：属性技（45 载体）
constexpr int kCanYing = 20336;  // 真实残影 [45]|[3]：属性技（45 载体）
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;   // 全程去暴击噪声（make_pet 默认已 0，双保险）
    }
}

// self：{载体, 拍打×4}，速度 300 先手、atk=1（filler 无伤害面）、本体 def 面板 400。
// foe：{拍打×5}，速度 100、atk=300、def=100。
ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1, kSelfHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 300, kFoeHp);
    accuracy_100(p);
    return p;
}

// 跑一场"对手每回合拍打 self"的局，返回 self 每回合掉血（下标 0 = R1）。
// carrier>0：R1 用载体、其余回合拍打 filler；carrier==0：全拍打（对照场）。
std::vector<int> run_battle(int carrier, int rounds, int self_def_panel) {
    ScenarioBattle b(make_party(mk_self(carrier > 0 ? carrier : kPaiDa)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (self_def_panel != 100) {
        ctx.getPet(0).numericalProperties[NumericalPropertyIndex::DEFENSE] = self_def_panel;
    }
    std::vector<int> losses;
    int prev = ctx.getPet(0).hp;
    for (int r = 0; r < rounds; ++r) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL,
                           (carrier > 0 && r == 0) ? 0 : 1);   // R1 载体，其余拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int now = ctx.getPet(0).hp;
        losses.push_back(prev - now);
        prev = now;
    }
    return losses;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261024);

    // ══ T7020 模仿术 [5]：窗内同防、窗过期回落 ══
    {
        const auto a = run_battle(kMoFang, 7, 400);   // R1 载体 → R2..R6 窗内，R7 过期
        const auto c = run_battle(0, 2, 100);         // 对照：def=100 原生，无载体
        const int r2 = a[1], r7 = a[6], native = c[1];
        std::cout << "  [dbg] copy_r2=" << r2 << " copy_r7=" << r7
                  << " native100=" << native << " r1=" << a[0] << "\n";
        CHECK(r2 > 60 && r2 < 130,
              "T7020.a1 模仿术：R2 掉血应落 def=100 拍打带 [60,130]（实测 " +
                  std::to_string(r2) + "）");
        CHECK(r2 * 10 > native * 8 && r2 * 10 < native * 13,
              "T7020.a2 模仿术：R2 与 def=100 原生同级（" + std::to_string(r2) +
                  " vs " + std::to_string(native) + "）");
        CHECK(r7 * 2 < r2,
              "T7020.b 模仿术：R7 窗过期回落 def=400 量级（" + std::to_string(r7) +
                  " < 一半 " + std::to_string(r2 / 2) + "）");
        std::cout << "  [T7020] 模仿术 [5] 防御同调 通过\n";
    }

    // ══ T7021 真实残影 [3]：窗长读 args（3 回合 → R4 起回落）══
    {
        const auto a = run_battle(kCanYing, 5, 400);  // R1 载体 → R2..R3 窗内，R4 起过期
        const int r2 = a[1], r5 = a[4];
        std::cout << "  [dbg] r2=" << r2 << " r5=" << r5 << "\n";
        CHECK(r2 > 60 && r2 < 130,
              "T7021.a 真实残影：R2 掉血应落 def=100 拍打带 [60,130]（实测 " +
                  std::to_string(r2) + "）");
        CHECK(r5 * 2 < r2,
              "T7021.b 真实残影：R5（窗长 3 已过期）回落 def=400 量级（" +
                  std::to_string(r5) + " < 一半 " + std::to_string(r2 / 2) + "）");
        std::cout << "  [T7021] 真实残影 [3] 窗长读 args 通过\n";
    }

    std::cout << "scenario_7020 PASS (J251 防御同调族 45)" << std::endl;
    return 0;
}
