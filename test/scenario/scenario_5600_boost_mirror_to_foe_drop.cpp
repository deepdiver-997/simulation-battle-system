// 场景 5600 —— J229 强化镜像赋予族（722，moves_lib/j_boost_mirror_to_foe_drop.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5600-5609）
//
// 断言口径（精确槽位快照，效果前后逐槽比对）：
//   - 722（大地哀嚎击 17359 纯链 [722]）：自身正等级逐槽 → 对手同槽等量下降。
//   - a) 自身 攻+2 / 防+1 / 速+6 → 对手 攻-2 / 防-1 / 速-6；自身等级不清除；
//        其余槽（特攻/特防/命中）双方向均 0。
//   - b) 自身无提升 → 对手全槽 0（不空放）。
//   - c) 对手免弱（STAT_DROP 票）→ 全部弱化被 IMMUNE，对手等级不变、自身提升仍在。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物 40（填充刀）
constexpr int kDaDi   = 17359;  // [722]（纯链，物理 150）
constexpr int kMaxHp  = 99999;
constexpr int kAtkSlot = 0, kSpaSlot = 1, kDefSlot = 2, kSpdSlot = 3, kSpeSlot = 4,
              kHitSlot = 5;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kDaDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
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

    // ══ T5600 722 自身提升 → 对手同槽下降 ══
    {
        {   // (a) 三正槽镜像；其余槽不动；自身不清除
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, kAtkSlot, 2) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 0, kDefSlot, 1) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 0, kSpeSlot, 6) == StatChangeResult::SUCCESS);
            cast(b);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], -2,
                     "T5600.a 722：自身攻+2 → 对手攻-2（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
            CHECK_EQ(ctx.ability_levels[1][kDefSlot], -1,
                     "T5600.a 722：自身防+1 → 对手防-1");
            CHECK_EQ(ctx.ability_levels[1][kSpeSlot], -6,
                     "T5600.a 722：自身速+6 → 对手速-6（钳制边界）");
            CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2, "T5600.a 722：自身攻+2 不清除");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], 1, "T5600.a 722：自身防+1 不清除");
            CHECK_EQ(ctx.ability_levels[0][kSpeSlot], 6, "T5600.a 722：自身速+6 不清除");
            CHECK_EQ(ctx.ability_levels[1][kSpaSlot], 0, "T5600.a 722：对手特攻槽仍 0");
            CHECK_EQ(ctx.ability_levels[1][kSpdSlot], 0, "T5600.a 722：对手特防槽仍 0");
            CHECK_EQ(ctx.ability_levels[1][kHitSlot], 0, "T5600.a 722：对手命中槽仍 0");
        }
        {   // (b) 自身无提升 → 不空放
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            cast(b);
            int nonzero = 0;
            for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
                nonzero += (ctx.ability_levels[1][i] != 0) ? 1 : 0;
            }
            CHECK_EQ(nonzero, 0,
                     "T5600.b 722：自身无提升 → 对手全槽 0（实测非零槽 " +
                         std::to_string(nonzero) + "）");
        }
        {   // (c) 对手免弱 → 全部 IMMUNE，双方等级不变
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/9);
            REQUIRE(stat_change(&ctx, 0, kAtkSlot, 2) == StatChangeResult::SUCCESS);
            REQUIRE(stat_change(&ctx, 0, kDefSlot, 1) == StatChangeResult::SUCCESS);
            cast(b);
            CHECK_EQ(ctx.ability_levels[1][kAtkSlot], 0,
                     "T5600.c 722：对手免弱 → 攻槽不变（实测 " +
                         std::to_string(ctx.ability_levels[1][kAtkSlot]) + "）");
            CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T5600.c 722：对手免弱 → 防槽不变");
            CHECK_EQ(ctx.ability_levels[0][kAtkSlot], 2, "T5600.c 722：自身提升仍在（攻+2）");
            CHECK_EQ(ctx.ability_levels[0][kDefSlot], 1, "T5600.c 722：自身提升仍在（防+1）");
        }
        std::cout << "  [T5600] 722 逐槽镜像 / 无提升空放 / 对手免弱 通过\n";
    }

    std::cout << "scenario_5600 PASS (J229 强化镜像赋予族)" << std::endl;
    return 0;
}
