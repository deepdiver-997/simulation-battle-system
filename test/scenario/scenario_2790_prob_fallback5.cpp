#include <utils/sbs_rand.h>
// 场景 2790 —— J177 申报兜底族三（2 宏，moves_lib/j_prob_fallback5.cpp）
//（2026-10-04，worktree ../agents/j177；号段 2790-2799 → T2790-T2791 两块）
//
// 断言口径（crit=0；atk=1 红伤不扰等级断言）：
//   - 1007（狂力弑天 [3,70,6,1] cat4 pri0 纯武装；链 191/843✓）：
//     R1 载体武装（属性技不算攻击门）→ R2-R4 拍打（窗口内每攻 70% 异常/30% 全降-1）：
//     对手 slot0 ∈ [-3,0]（逐层累计）；R5（窗过期）拍打不再变化（确定性过期断言）。
//   - 1318（末世丧钟 [5,1] 特140 pri0 纯链，p=5% 秒杀）：100 局各 1 回合——
//     击杀数 ∈ [0,10]（B(100,0.05) 上尾 ~1.4%）；非杀局全属性 -1（兜底确定面）。
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
constexpr int kKuangLi = 25061;  // 狂力弑天 [191,1007,843]|[3,70,6,1]：属性技
constexpr int kMoShi   = 32910;  // 末世丧钟 [1318]|[5,1]：特140
constexpr int kSelfHp  = 5000;
constexpr int kFoeHp   = 99999;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
        s.critical_strike_rate = 0.0f;
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

    // ══ T2790 1007 窗内每攻 roll/兜底（狂力弑天）══
    {
        ScenarioBattle b(
            make_party(mk_pet(1, "己方", {kKuangLi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
            make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体武装
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        for (int r = 0; r < 3; ++r) {   // R2-R4 窗内拍打
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
        }
        const int slot0_win = ctx.ability_levels[1][0];
        CHECK(slot0_win <= 0 && slot0_win >= -3,
              "T2790.a 1007：R2-R4 兜底累计 -0~-3（实测 " + std::to_string(slot0_win) + "）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R5（窗过期）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], slot0_win,
                 "T2790.a 1007：R5 窗过期不再兜底（实测 " +
                     std::to_string(ctx.ability_levels[1][0]) + "）");
        std::cout << "  [T2790] 1007 窗内每攻 roll/兜底 通过（slot0=" << slot0_win
                  << "）\n";
    }

    // ══ T2791 1318 概率秒杀/兜底全降（末世丧钟）══
    {
        constexpr int kTrials = 100;
        int kills = 0;
        for (int t = 0; t < kTrials; ++t) {
            ScenarioBattle b(
                make_party(mk_pet(1, "己方", {kMoShi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 1)),
                make_party(mk_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (ctx.getPet(1).hp <= 0) {
                ++kills;   // 秒杀落地
            } else {
                CHECK_EQ(ctx.ability_levels[1][0], -1,
                         "T2791 1318：非杀局兜底全属性-1（局 " + std::to_string(t) +
                             "，实测 " + std::to_string(ctx.ability_levels[1][0]) + "）");
            }
        }
        CHECK(kills <= 10,
              "T2791 1318 秒杀带：100 局击杀 " + std::to_string(kills) +
                  " 应 ≤10（B(100,0.05) 上尾）");
        std::cout << "  [T2791] 1318 概率秒杀/兜底 通过（击杀 " << kills << "/" << kTrials
                  << "）\n";
    }

    std::cout << "scenario_2790 PASS (J177 申报兜底族三 1007/1318)" << std::endl;
    return 0;
}
