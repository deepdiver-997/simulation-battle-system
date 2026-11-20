// 场景 7330 —— J256 下降镜像返还族（706，moves_lib/j_drop_return_family.cpp）
//（2026-10-24，worktree feat/j256；号段 7330-7339）
//
// 断言口径（精确槽位快照，J229 5600 同款）：
//   - a) 纯链江河倒灌 18334 [2]：干净开局首用 → 对手全槽 0（不空放）；自身
//        攻-2/速-1 再用 → 对手攻-4/速-2（2 倍）；自身不清除；其余槽双方向 0。
//   - b) 钳制：自身命中-4 → 返还 -8 → 对手命中钳 -6。
//   - c) 对手免弱（STAT_DROP 票）→ 全部 IMMUNE，双方等级不变。
//   - d) 混链天佑明尊 23837 [48,706,707]：706 切 [2]——自身防-1 → 对手防-2
//        （切片落位钉子；48/707 registry 已实现、链全活，无碍断言）。
//   - e) 混链灼锋破晦 37793 [2220,706,2200]：706 切 [10]（2220 arg_count=0 让参）
//        ——自身特攻-1 → 对手特攻-10 → 钳 -6（若误切 [2] 只会 -2，钉死切片）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物 40（填充刀）
constexpr int kJiangHe = 18334; // [706] 纯链（必中特攻 150）
constexpr int kTianYou = 23837; // [48,706,707] 混链（属性技）
constexpr int kZhuoFeng = 37793; // [2220,706,2200] 混链（必中物 150）
constexpr int kMaxHp = 99999;
constexpr int kAtkSlot = 0, kSpaSlot = 1, kDefSlot = 2, kSpdSlot = 3, kSpeSlot = 4,
              kHitSlot = 5;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p);
    return p;
}

void cast(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7330.a 纯链 18334 [2]：不空放 + 2 倍返还 + 自身不清除 ══
    {
        ScenarioBattle b(make_party(mk_self(kJiangHe)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        cast(b);   // 干净开局首用 → 不空放
        int nonzero = 0;
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            nonzero += (ctx.ability_levels[1][i] != 0) ? 1 : 0;
        }
        CHECK_EQ(nonzero, 0, "T7330.a 自身无下降 → 对手全槽 0（实测非零槽 " +
                                 std::to_string(nonzero) + "）");
        REQUIRE(stat_change(&ctx, 0, kAtkSlot, -2) == StatChangeResult::SUCCESS);
        REQUIRE(stat_change(&ctx, 0, kSpeSlot, -1) == StatChangeResult::SUCCESS);
        cast(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -4,
                 "T7330.a 自身攻-2 → 对手攻-4（2 倍，实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[1][kSpeSlot], -2,
                 "T7330.a 自身速-1 → 对手速-2");
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], -2, "T7330.a 自身攻-2 不清除");
        CHECK_EQ(ctx.ability_levels[0][kSpeSlot], -1, "T7330.a 自身速-1 不清除");
        CHECK_EQ(ctx.ability_levels[1][kSpaSlot], 0, "T7330.a 对手特攻槽仍 0");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T7330.a 对手防槽仍 0");
        std::cout << "  [T7330.a] 706 纯链[2] 2倍返还/不清除/不空放 通过\n";
    }

    // ══ T7330.b 钳制：返还 -8 → 对手命中钳 -6 ══
    {
        ScenarioBattle b(make_party(mk_self(kJiangHe)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kHitSlot, -4) == StatChangeResult::SUCCESS);
        cast(b);
        CHECK_EQ(ctx.ability_levels[1][kHitSlot], -6,
                 "T7330.b 返还 -8 应钳 -6（实测 " +
                     std::to_string(ctx.ability_levels[1][kHitSlot]) + "）");
        std::cout << "  [T7330.b] 706 返还钳 -6 通过\n";
    }

    // ══ T7330.c 对手免弱 → 全部 IMMUNE ══
    {
        ScenarioBattle b(make_party(mk_self(kJiangHe)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/9);
        REQUIRE(stat_change(&ctx, 0, kAtkSlot, -2) == StatChangeResult::SUCCESS);
        cast(b);
        CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                 "T7330.c 对手免弱 → 攻槽不变（实测 " +
                     std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[0][kAtkSlot], -2, "T7330.c 自身下降仍在");
        std::cout << "  [T7330.c] 706 对手免弱 IMMUNE 通过\n";
    }

    // ══ T7330.d 混链天佑明尊：706 切 [2] ══
    {
        ScenarioBattle b(make_party(mk_self(kTianYou)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kDefSlot, -1) == StatChangeResult::SUCCESS);
        cast(b);
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -2,
                 "T7330.d 天佑明尊 706 切 [2]：防-1 → 对手防-2（实测 " +
                     std::to_string(ctx.ability_levels[1][kDefSlot]) + "）");
        CHECK_EQ(ctx.ability_levels[0][kDefSlot], -1, "T7330.d 自身防-1 不清除");
        std::cout << "  [T7330.d] 706 混链天佑明尊切[2] 通过\n";
    }

    // ══ T7330.e 混链灼锋破晦：706 切 [10]（2220 arg_count=0 让参）→ 钳 -6 ══
    {
        ScenarioBattle b(make_party(mk_self(kZhuoFeng)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kSpaSlot, -1) == StatChangeResult::SUCCESS);
        cast(b);
        CHECK_EQ(ctx.ability_levels[1][kSpaSlot], -6,
                 "T7330.e 灼锋破晦 706 切 [10]：特攻-1 → 返还 -10 → 钳 -6"
                 "（实测 " + std::to_string(ctx.ability_levels[1][kSpaSlot]) +
                     "；若误切 [2] 只会 -2）");
        std::cout << "  [T7330.e] 706 混链灼锋破晦切[10]+钳制 通过\n";
    }

    std::cout << "scenario_7330 PASS (J256 下降镜像返还族)" << std::endl;
    return 0;
}
