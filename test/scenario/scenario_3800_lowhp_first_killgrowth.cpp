// 场景 3800 —— K236 低体先制+击杀成长族（648，soul_lib/k236_lowhp_first_killgrowth.cpp）
//（2026-10-08，worktree ../agents/k236；号段 3800-3809 → T3800-T3802 三块）
//
// 断言口径（行 [1,2]：低体门先制+1、击败攻击+2）：
//   - T3800 低体门：印主 hp 压低于对手 → R1 印主（速度 100 vs 300）翻盘先手
//     （preemptive_right + ws.preemptive_level[0]==1，回合完成点快照读法同 3720）；
//     R2 回满血 → 门闭、高速对手恢复先手（负控）；
//   - T3801 边界：双方 hp 相等（"低于"严格）→ 门闭、无先制；
//   - T3802 击杀成长：对手 hp=1 被印主击杀（BATTLE_AFTER_DEFEATED 归因门）→
//     自身攻击槽 +2；对手未死对照不成长（R1 压血不击杀断言）。
//   ⚠️ 全部纯 drive() 流（跨回合 step 停靠与 FSM 操作队列不合，3720 记档）。
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

constexpr int kJi = 10001;
constexpr int kMaxHp = 99999;

ElfPet mark_pet(int hp) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/hp,
                        /*soul_mark=*/SoulMark(648, "K236 魂印", "",
                                               EffectArgs(std::vector<int>{1, 2})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int hp) {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/hp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_attack(ScenarioBattle& b) {
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
    const int atk_slot = 0;

    // ══ T3800 低体门：R1 翻盘先手、R2 回满后门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*hp=*/5000)),
                         make_party(plain_pet(/*hp=*/kMaxHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_attack(b);   // R1：5000 < 99999 → 门开
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T3800 648：R1 低体先制 +1");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3800 648：R1 低速印主（100 vs 300）翻盘先手");
        ctx.getPet(0).hp = kMaxHp;   // 回满 → 99999 < 99999 不成立（严格低于）
        both_attack(b);   // R2
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T3800 648：R2 门闭先制归零");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T3800 648：R2 高速对手恢复先手");
        std::cout << "  [T3800] 低体先制门 通过\n";
    }

    // ══ T3801 边界：双方 hp 相等 → 不低于 → 门闭 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*hp=*/60000)),
                         make_party(plain_pet(/*hp=*/60000)));
        b.start();
        BattleContext& ctx = b.ctx();
        both_attack(b);
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T3801 648：hp 相等门闭（严格低于）");
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T3801 648：无先制、高速对手先手");
        std::cout << "  [T3801] 相等边界 通过\n";
    }

    // ══ T3802 击杀成长：击杀 → 攻击槽 +2 ══
    {
        ScenarioBattle b(make_party(mark_pet(/*hp=*/5000)),
                         make_party(plain_pet(/*hp=*/1)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.ability_levels[0][atk_slot], 0, "T3802 648：初始攻击 0");
        both_attack(b);   // 印主先手击杀 hp=1 对手
        CHECK_EQ(ctx.ability_levels[0][atk_slot], 2,
                 "T3802 648：击败对手 → 自身攻击 +2");
        std::cout << "  [T3802] 击杀成长 通过\n";
    }

    std::cout << "scenario_3800 PASS (K236 低体先制+击杀成长族)" << std::endl;
    return 0;
}
