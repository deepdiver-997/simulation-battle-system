#include <utils/sbs_rand.h>
// 场景 1060 —— J21 条件全属性降族（857/577，moves_lib/j_cond_all_down.cpp）
//（2026-10-02，worktree ../agents/j21；号段 1060-1069 → T1060-T1061 两块）
//
// 断言口径：
//   - 857 先手档 chance=100 → 六槽 -1 精确等式；后手档 chance=50 用 **种子校准**
//     （T994 口径：载体 月汐之氲 属性技无伤害相位 → 掷点=校准后首个对局掷点，
//     必中/必歪两种子各一局）；免弱对照 = 免弱票全挡；
//   - 577 门槛两半 + 增量档：六槽精确等式（stat_change 铺自身提升）。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（填充）
constexpr int kXieDu   = 32471;   // 蝎毒魔针 [857]|[50,1,100,1]   物85（857）
constexpr int kYueXi   = 26972;   // 月汐之氲 [857]|[50,1,100,1]   属（857 属性技，种子校准用）
constexpr int kShaPu   = 15677;   // 沙瀑之舞 [577]|[5,-1]         物80（577 命中-1）
constexpr int kCanYing = 34005;   // 残影轻拂 [577]|[5,-3]         物140（577 命中-3）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "条件降者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void check_foe_all_eq(BattleContext& ctx, int v, const char* tag) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        CHECK_EQ(ctx.ability_levels[1][i], v, std::string(tag) + "：对手槽 " +
                     std::to_string(i) + " 应为 " + std::to_string(v));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1060 857 先手档/后手档（32471 蝎毒魔针；后手档用 月汐之氲 种子校准）══
    {
        {   // (a) 先手：走 {2}%=100 档 → 六槽全 -1
            ScenarioBattle b(make_party(self_pet({kXieDu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_eq(ctx, -1, "T1060.a 857 先手档");
        }
        {   // (b) 后手必歪种子：走 {0}%=50 档 → 掷歪 → 六槽不动
            ScenarioBattle b(make_party(self_pet({kYueXi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            int seed_fail = -1;
            for (int s = 1; s < 100000 && seed_fail < 0; ++s) {
                sbs_srand(s);
                if (sbs_rand() % 100 >= 50) {
                    seed_fail = s;
                }
            }
            REQUIRE(seed_fail > 0);
            ctx.seed_rng(seed_fail);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_eq(ctx, 0, "T1060.b 857 后手档掷歪");
        }
        {   // (c) 后手必中种子：50% 档掷中 → 六槽全 -1
            ScenarioBattle b(make_party(self_pet({kYueXi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            int seed_hit = -1;
            for (int s = 1; s < 100000 && seed_hit < 0; ++s) {
                sbs_srand(s);
                if (sbs_rand() % 100 < 50) {
                    seed_hit = s;
                }
            }
            REQUIRE(seed_hit > 0);
            ctx.seed_rng(seed_hit);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_eq(ctx, -1, "T1060.c 857 后手档掷中");
        }
        {   // (d) 免弱对照：先手档照常掷中，但 stat_drop 全被免弱挡下
            ScenarioBattle b(make_party(self_pet({kXieDu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.grant_immunity(1, ImmunityType::STAT_DROP, coverage_all(), /*anomaly_mask=*/0,
                               /*duration_rounds=*/9, /*source_id=*/9997,
                               /*soul_immunity=*/false, EffectScope::ON_STAGE,
                               /*counts=*/0, /*source_effect_id=*/9997);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            check_foe_all_eq(ctx, 0, "T1060.d 857 免弱对照");
        }
        std::cout << "  [T1060] 857 先手/后手档+免弱 通过\n";
    }

    // ══ T1061 577 提升门（15677 沙瀑之舞 [5,-1] / 34005 残影轻拂 [5,-3]）══
    {
        {   // (a) 自身有提升 → 对手命中 -1
            ScenarioBattle b(make_party(self_pet({kShaPu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, +1) == StatChangeResult::SUCCESS);   // 自身攻+1
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][5], -1, "T1061.a 577：有提升 → 对手命中 -1");
            CHECK_EQ(ctx.ability_levels[0][0], 1, "T1061.a 前置：自身提升在（不消耗）");
        }
        {   // (b) 自身无提升 → 门关
            ScenarioBattle b(make_party(self_pet({kShaPu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][5], 0, "T1061.b 577 门槛：无提升不降");
        }
        {   // (c) 增量档 [5,-3]
            ScenarioBattle b(make_party(self_pet({kCanYing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 4, +2) == StatChangeResult::SUCCESS);   // 自身速+2
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][5], -3, "T1061.c 577：增量 -3 档");
        }
        std::cout << "  [T1061] 577 提升门 通过\n";
    }

    std::cout << "scenario_1060 PASS (J21 条件全属性降族)" << std::endl;
    return 0;
}
