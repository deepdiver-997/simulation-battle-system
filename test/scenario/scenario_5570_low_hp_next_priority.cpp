// 场景 5570 —— J226 低体下回合先制族（1230，moves_lib/j_low_hp_next_priority.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5570-5579）
//
// 断言口径（先手权翻转 + 严格低体门 + 窗界）：
//   - 1230（幽心萧瑟 28024 [191,1230]|[3,1,2]）：{0}=1（下 1 次≈下回合）、{1}=+2。
//     载体速度 99 vs 对手 100——无条件时对手恒先手；+2 后自身翻盘，判据唯一。
//   - a) 自身 1000 < 对手 99999 → R2 自身先手（SEER_ROBOT_1）；R3 恢复对手先手。
//   - b) 自身 99999 > 对手 5000 → 门闭，R2 仍对手先手。
//   - c) 体力相等 → "低于"严格门闭，R2 仍对手先手。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物 40（填充刀，atk=1 无致死风险）
constexpr int kYouXin = 28024;  // [191,1230]|[3,1,2]（cat4 属性技）
constexpr int kSelfSpd = 99;    // 恒慢于对手 100（无条件时对手先手）
constexpr int kFoeSpd  = 100;
constexpr int kMaxHp  = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kYouXin, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kSelfSpd, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/kFoeSpd, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void cast_scenario(ScenarioBattle& b, bool self_casts) {
    b.submit_operation(0, self_casts ? BattleFsm::ActionType::SELECT_SKILL
                                     : BattleFsm::ActionType::NONE,
                       0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 槽 1=拍打（槽 0 是 1230 载体）
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5570 1230 低体 → 下回合先制 +2 ══
    {
        {   // (a) 低体门开：R2 自身先手；R3 窗过期恢复对手先手
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;   // 只压当前体力，最大体力不变
            cast_scenario(b, /*self_casts=*/true);
            both_attack(b);
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_1),
                     "T5570.a 1230：自身 1000 < 对手 99999 → R2 自身先制 +2 翻盘");
            both_attack(b);
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T5570.a 1230：下 1 次窗过期 → R3 恢复对手先手");
        }
        {   // (b) 高体门闭
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = 5000;   // 自身 99999 > 对手 5000
            cast_scenario(b, /*self_casts=*/true);
            both_attack(b);
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T5570.b 1230：自身高体 → 门闭，R2 仍对手先手");
        }
        {   // (c) 相等门闭（严格"低于"）
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 5000;
            ctx.getPet(1).hp = 5000;
            cast_scenario(b, /*self_casts=*/true);
            both_attack(b);
            CHECK_EQ(static_cast<int>(ctx.preemptive_right),
                     static_cast<int>(PreemptiveRight::SEER_ROBOT_2),
                     "T5570.c 1230：体力相等 → 严格门闭，R2 仍对手先手");
        }
        std::cout << "  [T5570] 1230 低体先制 / 高体门闭 / 相等门闭 / 窗过期 通过\n";
    }

    std::cout << "scenario_5570 PASS (J226 低体下回合先制族)" << std::endl;
    return 0;
}
