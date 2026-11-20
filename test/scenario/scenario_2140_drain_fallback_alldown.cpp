#include <utils/sbs_rand.h>
// 场景 2140 —— J120 吸强兜底全属性降族（1 宏，moves_lib/j_drain_fallback_alldown.cpp）
//（2026-10-03，worktree ../agents/j120；号段 2140-2149 → T2140 一块）
//
// 断言口径（走能力等级面——对链 mate 999 的未来激活免疫，不碰伤害数值）：
//   载体 冥域妖极天 30621 [1127,999]|[1,1,10] 物160 prio0（999 未实现哑）。
//   a) 对手预置 攻+2 → 载体命中 → 强化归我（self slot0=+2）+ 对手全属性-1
//      （吸取后正槽已空 → 兜底生效：opp slot0=-1、slot4=-1）
//   b) 对照：对手无提升 → 吸取无所得（self 全 0）+ 兜底仍落（opp 全 -1）
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40 基线
constexpr int kMingYu = 30621;  // 冥域妖极天 [1127,999]|[1,1,10]：物160（1127 载体，prio0）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, kMaxHp);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 99999);
    accuracy_100(p);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(20261003);

    const std::array<int, 5> kit = {kMingYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa};

    // ══ T2140.a 对手 +2 → 吸取归我 + 兜底全属性-1 ══
    {
        ScenarioBattle b(make_party(mk_self(kit)),
                         make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)stat_change(&ctx, 1, 0, 2);   // 对手攻击 +2
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 载体
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.ability_levels[0][0] == 2,
              "T2140.a 1127：强化归我（self slot0 实测 " +
                  std::to_string(ctx.ability_levels[0][0]) + "，应 2）");
        CHECK(ctx.ability_levels[1][0] == -1 && ctx.ability_levels[1][4] == -1,
              "T2140.a 1127：兜底全属性-1（opp slot0/slot4 实测 " +
                  std::to_string(ctx.ability_levels[1][0]) + "/" +
                  std::to_string(ctx.ability_levels[1][4]) + "，应 -1/-1）");
        std::cout << "  [T2140.a] 吸取归我+兜底生效 通过\n";
    }

    // ══ T2140.b 对照：对手无提升 → 无所得 + 兜底仍落 ══
    {
        ScenarioBattle b(make_party(mk_self(kit)),
                         make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.ability_levels[0][0] == 0,
              "T2140.b 1127 对照：无提升不吸（self slot0 实测 " +
                  std::to_string(ctx.ability_levels[0][0]) + "，应 0）");
        CHECK(ctx.ability_levels[1][0] == -1 && ctx.ability_levels[1][4] == -1,
              "T2140.b 1127 对照：兜底仍落（opp slot0/slot4 实测 " +
                  std::to_string(ctx.ability_levels[1][0]) + "/" +
                  std::to_string(ctx.ability_levels[1][4]) + "，应 -1/-1）");
        std::cout << "  [T2140.b] 无提升对照 通过\n";
    }

    std::cout << "scenario_2140 PASS (J120 吸强兜底全属性降族 1127)" << std::endl;
    return 0;
}
