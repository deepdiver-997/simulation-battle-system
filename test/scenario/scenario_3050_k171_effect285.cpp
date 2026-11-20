// 场景 3050 —— K171 满体力条件免疫族（285，soul_lib/k_fullhp_immune_family.cpp）
//（2026-10-07，worktree ../agents/k171；号段 3050-3059 → T3050-T3052 三块）
//
// 断言口径（2880/221 同款时序：enter 票在 b.start() 停泊点已挂，直铺异常/弱化，
// 全程不跨回合、无停泊步进；50 档统计带侧证）：
//   - T3050 行1 [100,50] 免疫情形：满血直铺烧伤被免（apply 拒 + 无异常）→
//     掉血直铺落地（票在、condition false 不误挡）→ 回满直铺又被免（现场判动态）；
//   - T3051 行2 [100,100] 免降确定档：满血 stat_drop IMMUNE（levels 不动）→
//     掉血 SUCCESS -1；
//   - T3052 行1 免降 50 档统计带：满血反复铺（每轮清 levels）IMMUNE 率 ∈ [30,70]。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2
constexpr int kParalyze = static_cast<int>(AbnormalStatusId::Paralysis);   // 0
constexpr int kMaxHp = 99999;

ElfPet mark_pet(const std::array<int, 5>& skills, const std::vector<int>& row_args) {
    ElfPet p = make_pet(1, "印主", skills, /*speed=*/300, /*atk=*/1, kMaxHp,
                        /*soul_mark=*/SoulMark(285, "K171 魂印", "",
                                               EffectArgs(
                                                   std::vector<int>{row_args[0],
                                                                    row_args[1]})));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(const std::array<int, 5>& skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void set_hp(BattleContext& ctx, int hp) {
    ctx.seerRobot[0].elfPets[0].hp = hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3050 行1 [100,50]：免疫情形（满血免 → 掉血落地）══
    {
        ScenarioBattle b(make_party(mark_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             {100, 50})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1),
              "T3050 满体力 → 100% 免异常（直铺被拒）");
        CHECK(!ctx.has_active_abnormal_status(0, kBurn), "T3050 满血无异常");
        set_hp(ctx, 5000);
        REQUIRE(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1));
        CHECK(ctx.has_active_abnormal_status(0, kBurn),
              "T3050 掉血 → 免疫失效烧伤落地（票在、condition false 不误挡）");
        std::cout << "  [T3050] 285 行1 免疫情形 通过\n";
    }
    // 回满恢复免控（独立对局：掉血落地 → 回满 → 换异常再铺）
    {
        ScenarioBattle b(make_party(mark_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             {100, 50})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        set_hp(ctx, 5000);
        REQUIRE(try_apply_anomaly(&ctx, 0, kBurn, /*duration=*/3, /*actor=*/1));
        CHECK(ctx.has_active_abnormal_status(0, kBurn), "T3050b 掉血烧伤落地");
        set_hp(ctx, kMaxHp);
        CHECK(!try_apply_anomaly(&ctx, 0, kParalyze, /*duration=*/3, /*actor=*/1),
              "T3050b 回满 → 免控恢复（condition 现场判动态回正）");
        CHECK(!ctx.has_active_abnormal_status(0, kParalyze), "T3050b 无麻痹");
        std::cout << "  [T3050b] 回满恢复免控 通过\n";
    }

    // ══ T3051 行2 [100,100]：免降确定档（满血 IMMUNE → 掉血 -1）══
    {
        ScenarioBattle b(make_party(mark_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             {100, 100})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_drop(&ctx, 0, 0, 1, 1) == StatDropResult::IMMUNE);
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T3051 满血免降（直铺被免）");
        set_hp(ctx, 5000);
        REQUIRE(stat_drop(&ctx, 0, 0, 1, 1) == StatDropResult::SUCCESS);
        CHECK_EQ(ctx.ability_levels[0][0], -1, "T3051 掉血 → -1 落地");
        std::cout << "  [T3051] 285 行2 免降确定档 通过\n";
    }

    // ══ T3052 行1 [100,50]：免降 50 档统计带（p=0.5，70 轮 ∈ [30,70]≈±5σ）══
    {
        ScenarioBattle b(make_party(mark_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                             {100, 50})),
                         make_party(plain_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int immune = 0;
        constexpr int kTrials = 70;
        for (int i = 0; i < kTrials; ++i) {
            ctx.ability_levels[0][0] = 0;   // 每轮清级，AT_FLOOR 不污染统计
            if (stat_drop(&ctx, 0, 0, 1, 1) == StatDropResult::IMMUNE) {
                ++immune;
            }
        }
        const int pct = immune * 100 / kTrials;
        CHECK(pct >= 30 && pct <= 70,
              "T3052 免降 50 档统计带越界：immune=" << immune << "/" << kTrials);
        std::cout << "  [T3052] 285 行1 免降 50 档统计 " << immune << "/" << kTrials
                  << " 通过\n";
    }

    std::cout << "scenario_3050 PASS (K171 满体力条件免疫族)" << std::endl;
    return 0;
}
