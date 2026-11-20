// 场景 5190 —— K349 回合条件护盾/属性族（941/951，soul_lib/k_round_shield_stat.cpp）
//（2026-10-22，worktree k349；号段 5190-5199 → T5190-T5192 三块）
//
// 断言口径（双方 NONE 出手，零红伤干扰 → 体力/等级变化全部来自魂印）：
//   - T5190 941：R1 盾 250 + 对手 6 槽各 -1；R2 盾 500（每回合叠加）+ 对手 -2；
//     精确读 shield_bank_.total() 与 ability_levels 六槽。
//   - T5191 951：自身体力高于对手 → 每回合 6 槽 +1（R1=1/R2=2）；
//     回合末回复 200（印主先压 1000 血，实测净 +200）+ 对手等量固定伤害 200/回合。
//   - T5192 无印对照：盾 0、等级 0、双方体力不变。
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kOwnHp = 200000;
constexpr int kFoeHp = 100000;

SoulMark make_mark(int id, std::vector<int> args) {
    SoulMark sm(id, "K349 魂印", "test", EffectArgs(std::move(args)));
    sm.monster_id = (id == 941) ? 3331 : 3346;
    return sm;
}

ElfPet mark_pet(int id, std::vector<int> args) {
    return make_pet(1, "印主", {10004, 10004, 10004, 10004, 10004},
                    /*speed=*/100, /*atk=*/100, kOwnHp,
                    make_mark(id, std::move(args)));
}

ElfPet foe_pet(int hp) {
    return make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004},
                    /*speed=*/100, /*atk=*/100, hp);
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

std::string levels6(const BattleContext& ctx, int side) {
    std::string s;
    for (int i = 0; i < 6; ++i) {
        s += " " + std::to_string(ctx.ability_levels[side][i]);
    }
    return s;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5190 941：每回合叠加 250 盾 + 回合末对手全属性 -1 ═══════════════════
    {
        ScenarioBattle b(make_party(mark_pet(941, {250, 1, 1})),
                         make_party(foe_pet(kFoeHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 250,
                 "T5190 941：R1 回合首护盾 250（实测盾值）");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -1,
                     "T5190 941：R1 对手全属性 -1（槽 " + std::to_string(i) +
                         "，实测" + levels6(ctx, 1) + "）");
        }
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 500,
                 "T5190 941：R2 护盾叠加至 500");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[1][i], -2,
                     "T5190 941：R2 对手全属性 -2（槽 " + std::to_string(i) +
                         "，实测" + levels6(ctx, 1) + "）");
        }
        std::cout << "  [T5190] 941 护盾叠加 + 全属性降级 通过\n";
    }

    // ══ T5191 951：高体 → 全属性 +1/回合；回合末回 200 + 等量固伤 ═══════════
    {
        ScenarioBattle b(make_party(mark_pet(951, {1, 200, 1})),
                         make_party(foe_pet(kFoeHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp -= 1000;   // 压血测回复（不撞满血封顶）
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).hp, kOwnHp - 1000 + 200,
                 "T5191 951：R1 回合末回复 200（实测 " +
                     std::to_string(ctx.getPet(0).hp) + "）");
        CHECK_EQ(ctx.getPet(1).hp, kFoeHp - 200,
                 "T5191 951：R1 对手等量固定伤害 200（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 1,
                     "T5191 951：R1 自身全属性 +1（槽 " + std::to_string(i) +
                         "，实测" + levels6(ctx, 0) + "）");
        }
        idle_round(b);
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 2,
                     "T5191 951：R2 自身全属性 +2（槽 " + std::to_string(i) +
                         "，实测" + levels6(ctx, 0) + "）");
        }
        CHECK_EQ(ctx.getPet(1).hp, kFoeHp - 400,
                 "T5191 951：R2 再造成 200 固伤（实测 " +
                     std::to_string(ctx.getPet(1).hp) + "）");
        std::cout << "  [T5191] 951 高体全属性 + 回合末回复/固伤 通过\n";
    }

    // ══ T5192 无印对照：零变化 ═════════════════════════════════════════════════
    {
        ElfPet bare = make_pet(3, "裸印主", {10004, 10004, 10004, 10004, 10004},
                               /*speed=*/100, /*atk=*/100, kOwnHp);
        ScenarioBattle b(make_party(bare), make_party(foe_pet(kFoeHp)));
        b.start();
        BattleContext& ctx = b.ctx();
        idle_round(b);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                 "T5192 无印：护盾 0");
        CHECK_EQ(ctx.getPet(0).hp, kOwnHp, "T5192 无印：印主体力不变");
        CHECK_EQ(ctx.getPet(1).hp, kFoeHp, "T5192 无印：对手体力不变");
        for (int i = 0; i < 6; ++i) {
            CHECK_EQ(ctx.ability_levels[0][i], 0,
                     "T5192 无印：自身等级 0（实测" + levels6(ctx, 0) + "）");
            CHECK_EQ(ctx.ability_levels[1][i], 0,
                     "T5192 无印：对手等级 0（实测" + levels6(ctx, 1) + "）");
        }
        std::cout << "  [T5192] 无印对照 通过\n";
    }

    std::cout << "scenario_5190 PASS (K349 回合条件护盾/属性族)" << std::endl;
    return 0;
}
