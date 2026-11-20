#include <utils/sbs_rand.h>
// 场景 3860 —— K242 双分支概率害怕/吸取族（1414，soul_lib/k242_hpbranch_fear_drain.cpp）
//（2026-10-13，worktree ../agents/k242；号段 3860-3869 → T3860-T3863 四块）
//
// 断言口径（行 [75,1,3,1]：HP 对照双分支，75%，未触发全属性-1）：
//   高体分支（60000 vs 50000）：触发=对手害怕落地（end_round>0、等级不动）；
//   未触发=六槽全 -1（含命中，chaos 口径）；低体分支（40000 vs 50000）：触发=
//   吸 33333（opp maxHP 99999/3，粉伤+同量回复，压血防封顶）；未触发=六槽全 -1。
//   75% 掷点运行期自校准（K12 2920 口径）；双方 NONE，唯一变量 = 印主 ROUND_END 结算。
//   ⚠️ 纯 drive() 流（3720 记档）。
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
constexpr int kFearId = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kSlots = 6;

ElfPet mark_pet() {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(1414, "K242 魂印", "",
                                               EffectArgs(std::vector<int>{75, 1, 3, 1})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

// 预置双方 hp（开战后），双方 NONE 过一回合（ROUND_END 结算）。
void round_with_hp(ScenarioBattle& b, int my_hp, int foe_hp) {
    b.ctx().getPet(0).hp = my_hp;
    b.ctx().getPet(1).hp = foe_hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

bool foe_all_dropped(BattleContext& ctx) {
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[1][i] != -1) {
            return false;
        }
    }
    return true;
}

bool foe_levels_untouched(BattleContext& ctx) {
    for (int i = 0; i < kSlots; ++i) {
        if (ctx.ability_levels[1][i] != 0) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // 自校准：高体分支（触发/未触发）与低体分支（触发/未触发）种子。
    int seed_hi_hit = -1, seed_hi_fail = -1, seed_lo_hit = -1, seed_lo_fail = -1;
    for (int s = 1; s <= 500 && (seed_hi_hit < 0 || seed_hi_fail < 0
                                 || seed_lo_hit < 0 || seed_lo_fail < 0); ++s) {
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            sbs_srand(s);
            round_with_hp(b, 60000, 50000);   // 高体
            BattleContext& ctx = b.ctx();
            if (ctx.abnormal_status_end_round[1][kFearId] > 0) {
                if (seed_hi_hit < 0) seed_hi_hit = s;
            } else if (seed_hi_fail < 0) {
                seed_hi_fail = s;
            }
        }
        {
            ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
            b.start();
            sbs_srand(s);
            round_with_hp(b, 40000, 50000);   // 低体
            BattleContext& ctx = b.ctx();
            if (ctx.getPet(1).hp < 50000) {
                if (seed_lo_hit < 0) seed_lo_hit = s;
            } else if (seed_lo_fail < 0) {
                seed_lo_fail = s;
            }
        }
    }
    CHECK(seed_hi_hit > 0 && seed_hi_fail > 0, "T3860 1414：高体双分支种子可达");
    CHECK(seed_lo_hit > 0 && seed_lo_fail > 0, "T3860 1414：低体双分支种子可达");
    std::cout << "  [3860] 自校准：高体(" << seed_hi_hit << "/" << seed_hi_fail
              << ") 低体(" << seed_lo_hit << "/" << seed_lo_fail << ")\n";

    // ══ T3860 高体-触发：害怕落地、等级不动 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        sbs_srand(seed_hi_hit);
        round_with_hp(b, 60000, 50000);
        CHECK(b.ctx().abnormal_status_end_round[1][kFearId] > 0,
              "T3860 1414：高体触发 → 对手害怕");
        CHECK(foe_levels_untouched(b.ctx()),
              "T3860 1414：触发档不降属性");
        std::cout << "  [T3860] 高体害怕分支 通过\n";
    }

    // ══ T3861 高体-未触发：六槽全 -1 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        sbs_srand(seed_hi_fail);
        round_with_hp(b, 60000, 50000);
        CHECK_EQ(b.ctx().abnormal_status_end_round[1][kFearId], 0,
                 "T3861 1414：未触发不害怕");
        CHECK(foe_all_dropped(b.ctx()), "T3861 1414：未触发 → 全属性-1");
        std::cout << "  [T3861] 高体未触发分支 通过\n";
    }

    // ══ T3862 低体-触发：吸 33333 + 回复同量 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        sbs_srand(seed_lo_hit);
        round_with_hp(b, 40000, 50000);
        BattleContext& ctx = b.ctx();
        CHECK_EQ(50000 - ctx.getPet(1).hp, kMaxHp / 3,
                 "T3862 1414：低体触发 → 吸取对手 1/3 maxHP");
        CHECK_EQ(ctx.getPet(0).hp - 40000, kMaxHp / 3,
                 "T3862 1414：自身回复同量");
        CHECK(foe_levels_untouched(ctx), "T3862 1414：触发档不降属性");
        std::cout << "  [T3862] 低体吸取分支 通过\n";
    }

    // ══ T3863 低体-未触发：六槽全 -1 ══
    {
        ScenarioBattle b(make_party(mark_pet()), make_party(plain_pet()));
        b.start();
        sbs_srand(seed_lo_fail);
        round_with_hp(b, 40000, 50000);
        CHECK_EQ(b.ctx().getPet(1).hp, 50000, "T3863 1414：未触发不吸取");
        CHECK(foe_all_dropped(b.ctx()), "T3863 1414：未触发 → 全属性-1");
        std::cout << "  [T3863] 低体未触发分支 通过\n";
    }

    std::cout << "scenario_3860 PASS (K242 双分支概率害怕/吸取族)" << std::endl;
    return 0;
}
