#include <utils/sbs_rand.h>
// 场景 3345 —— J180 消强成功升级族（430，moves_lib/j_dispel_boost_stat_up.cpp）
//（2026-10-07，worktree ../agents/j180，看板段 3345-3354 → T3345-T3347）
//
// 载体策略：直接用三技真身（链全接——427/155/709/1395/1472 registry 全✓，链上效果
// 按各自口径自然生效，均不污染本单断言：427 掉的是对手防/特防槽、1395 只翻倍暴击、
// 1472 需自身提升（无）、709 需击杀（无）、155 满血自愈无观感）。
//
// 断言口径：
//   - T3345 竭力大切割 37798（攻+2）：对手预置攻+2 → 出手后对手攻回 0（被消除）
//     且自身攻 +2（升级）；对照（对手无提升）→ 自身攻不动（消除失败短路）。
//   - T3346 半梦 38427（防+2，db[2,2]→槽2=防）：同门验证防御槽；155 链满血自愈
//     不影响等级断言。（⚠️ 立单行误写"特防+2"——槽序 0攻1特攻2防3特防，2=防，
//     以本记档对账。）
//   - T3347 破风头槌 38279（攻+2）：同 T3345（第二攻+2 载体互锚）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMaxHp = 99999;

ElfPet holder_pet(int skill_id, int speed) {
    ElfPet p = make_pet(1, "印主", {skill_id, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {10004, 10004, 10004, 10004, 10004}, speed,
                        /*atk=*/1, /*hp=*/kMaxHp);
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

    // ══ T3345 竭力大切割 37798（攻+2，含消除失败短路对照）══
    {
        for (int side = 0; side < 2; ++side) {
            ScenarioBattle b(make_party(holder_pet(37798, /*speed=*/300)),
                             make_party(plain_foe(/*speed=*/100)));
            sbs_srand(7);
            b.start();
            if (side == 0) {
                core_api().stat_change(&b.ctx(), /*target=*/1, /*stat=*/0, /*delta=*/2);
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            if (side == 0) {
                CHECK_EQ(b.ctx().ability_level(1, 0), 0,
                         "T3345 430：对手攻+2 被消除");
                CHECK_EQ(b.ctx().ability_level(0, 0), 2,
                         "T3345 430：消除成功 → 自身攻+2");
            } else {
                CHECK_EQ(b.ctx().ability_level(0, 0), 0,
                         "T3345 430：对手无提升 → 消除失败短路不给升级");
            }
        }
        std::cout << "  [T3345] 竭力大切割攻+2 通过\n";
    }

    // ══ T3346 半梦 38427（特防+2）══
    {
        ScenarioBattle b(make_party(holder_pet(38427, /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        core_api().stat_change(&b.ctx(), 1, 0, 2);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(b.ctx().ability_level(1, 0), 0, "T3346 430：对手攻被消除");
        CHECK_EQ(b.ctx().ability_level(0, 2), 2, "T3346 430：半梦防+2（db[2,2]→槽2=防）");
        std::cout << "  [T3346] 半梦防+2 通过\n";
    }

    // ══ T3347 破风头槌 38279（攻+2）══
    {
        ScenarioBattle b(make_party(holder_pet(38279, /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        core_api().stat_change(&b.ctx(), 1, 0, 2);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(b.ctx().ability_level(1, 0), 0, "T3347 430：对手攻被消除");
        CHECK_EQ(b.ctx().ability_level(0, 0), 2, "T3347 430：自身攻+2");
        std::cout << "  [T3347] 破风头槌攻+2 通过\n";
    }

    std::cout << "scenario_3345 PASS (J180 消强成功升级族)" << std::endl;
    return 0;
}
