// 场景 4360 —— K280 技能类型门自增益族（884，soul_lib/k280_skilltype_stat.cpp）
//（2026-10-21，worktree ../agents/k280；号段 4360-4369 → T4360-T4361 两块）
//
// 断言口径（884「使用攻击技能时自身防御+2；使用属性技能时自身特防+2」）：
//   - T4360 攻击技：印主用撞击（攻击技）→ **防御槽(2) +2**，特防槽(3) 不变。
//   - T4361 属性技：印主用瞪眼（属性技，effect 5=改**对手**等级，不动自身等级）
//     → **特防槽(3) +2**，防御槽(2) 不变。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传 {2,2}（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击技
constexpr int kSuoTou = 20004; // 瞪眼：属性技（effect 5=改对手等级，不动自身）
constexpr int kDefenseSlot = 2;    // 防御
constexpr int kSpDefenseSlot = 3; // 特防

// 印主：技能 0 = 使用的那个技。
ElfPet mark_pet(int skill_id) {
    std::vector<int> args{2, 2};
    ElfPet p = make_pet(1, "印主", {skill_id, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(884, "K280 魂印", "",
                                               EffectArgs(std::move(args))));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4360 攻击技 → 防御 +2 ══════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kJi)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kDefenseSlot], 2,
                 "T4360 884：用攻击技 → 自身防御等级 +2");
        CHECK_EQ(ctx.ability_levels[0][kSpDefenseSlot], 0,
                 "T4360 884：用攻击技 → 特防等级不变");
        std::cout << "  [T4360] 攻击技门 通过\n";
    }

    // ══ T4361 属性技 → 特防 +2 ══════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kSuoTou)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][kSpDefenseSlot], 2,
                 "T4361 884：用属性技 → 自身特防等级 +2");
        CHECK_EQ(ctx.ability_levels[0][kDefenseSlot], 0,
                 "T4361 884：用属性技 → 防御等级不变");
        std::cout << "  [T4361] 属性技门 通过\n";
    }

    std::cout << "scenario_4360 PASS (K280 技能类型门自增益族)" << std::endl;
    return 0;
}
