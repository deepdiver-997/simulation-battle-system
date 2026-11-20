// 场景 4010 —— K251 受击吸取提升/级联封属先制族（882，soul_lib/k251_hitsteal_seal_prio.cpp）
//（2026-10-13，worktree ../agents/k251；号段 4010-4019 → T4010-T4011 两块）
//
// 断言口径（行 [2,2,1,1]：吸强/封属双分支 + 先制窗，无掷点全确定性）：
//   · T4010 吸取+先制：对手 +2 强化攻击印主 → 强化被搬走（opp 归 0、own +2）；
//     R2/R3 印主先制 +1（ws 快照读法，3720 同款）、R4 归零；
//   · T4011 无强可吸 → 封属：对手属性技 R2/R3 被拦（EVENT_SKILL_ARMOR_RESOLVED
//     blocked 计数，3720 同观测面）、R4 不拦；先制恒 0。
//   ⚠️ 纯 drive() 流（3720 记档）。
#include <array>
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
constexpr int kSuoTou = 20001;   // 缩头（属性技载体，封属拦截观测用）
constexpr int kMaxHp = 99999;
constexpr int kAtkSlot = 0;

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(882, "K251 魂印", "",
                                               EffectArgs(std::vector<int>{2, 2, 1, 1})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 攻击载体（T4010）/ 属性载体（T4011 封属观测）。
ElfPet foe_pet(bool with_attribute) {
    const std::array<int, 5> skills = with_attribute
        ? std::array<int, 5>{kJi, kJi, kJi, kJi, kSuoTou}
        : std::array<int, 5>{kJi, kJi, kJi, kJi, kJi};
    ElfPet p = make_pet(2, "对手", skills,
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_hits(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void round_none(ScenarioBattle& b) {
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

    // ══ T4010 吸取 + 先制窗（R2/R3 活、R4 过）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(false)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kAtkSlot] = 2;
        foe_hits(b);   // R1：吸取（对手强化 → 自身）
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0, "T4010 882：对手强化被吸走");
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2, "T4010 882：自身获得强化");
        round_none(b);   // R2
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T4010 882：R2 先制 +1");
        round_none(b);   // R3
        CHECK_EQ(ctx.ws.preemptive_level[0], 1, "T4010 882：R3 先制 +1（窗内）");
        round_none(b);   // R4
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T4010 882：R4 窗过期");
        std::cout << "  [T4010] 吸取+先制窗 通过\n";
    }

    // ══ T4011 无强可吸 → 封属下2回合（R2/R3 拦、R4 不拦）══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(foe_pet(true)));
        b.start();
        BattleContext& ctx = b.ctx();
        int sealed = 0;
        ctx.event_center_.register_watcher(
            EventType::EVENT_SKILL_ARMOR_RESOLVED, 0, ctx.roundCount,
            /*duration_rounds=*/0, /*once=*/false,
            [&sealed](BattleContext*, const BattleEvent& ev) {
                if (ev.target == 1 && ev.blocked) {
                    ++sealed;
                }
            },
            WatcherScope::TEAM);
        foe_hits(b);   // R1：对手无提升 → 封属账本
        CHECK_EQ(ctx.ws.preemptive_level[0], 0, "T4011 882：未吸取无先制");
        for (int r = 2; r <= 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 4);
            b.drive();
            CHECK_EQ(sealed, r <= 3 ? r - 1 : 2,
                     (r <= 3 ? "T4011 882：R" : "T4011 882：R4 窗过期（累计 ")
                         << r << " 封属拦截");
        }
        std::cout << "  [T4011] 无强封属分支 通过\n";
    }

    std::cout << "scenario_4010 PASS (K251 受击吸取提升/级联封属先制族)" << std::endl;
    return 0;
}
