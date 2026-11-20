// 场景 2470 —— J152 MISS/反转/减伤杂门（3 宏，moves_lib/j_miss_reversal_reduce.cpp）
//（2026-10-04，worktree ../agents/j152；号段 2470-2479 → T2470 一块四例）
//
// 断言口径（红伤带 = 0.84×P×A/D；kMaxHp=800000）：
//   - a 177（光佑守护 [5,2] 属性技）：R1 自方武装+吃大刀（atk=720000 →
//     [205632,241920]）；R2 起对手 acc=0 必 miss → ROUND_END 回 1/2 max
//     （缺口 < 40 万 → 直接回满）；
//   - b 961（死寂厄运咒 [2] 物150）：预挂物攻-1 → R1 反转成功（槽位 -1→+1 钉死）
//     + 下 2 回合免攻票 → R2/R3 吃大刀掉血 0、R4 窗过期吃满带；负例（无降槽）
//     反转失败不挂窗；
//   - c 522（初发芙蓉 [3,300] 特140）：自方预中毒 → R2/R3 对手拍打（atk=900 →
//     红伤 [257,302]）被减 300 → 掉血 ≤2；R4 窗过期吃满带；负例（无异常）不减。
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

constexpr int kGuangYou = 21543;  // 光佑守护 [177]|[5,2]：属性技纯链
constexpr int kSiJi     = 19688;  // 死寂厄运咒 [961]|[2]：物150 纯链
constexpr int kFuRong   = 34285;  // 初发芙蓉 [522]|[3,300]：特140 纯链
constexpr int kPaiDa    = 10004;
constexpr int kPoison   = static_cast<int>(AbnormalStatusId::Poison);  // 1
constexpr int kMaxHp    = 800000;

void tune(ElfPet& p, int acc, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = acc;
        s.must_hit = must_hit ? 1 : 0;
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2470.a 177：对手 MISS → 回合末回 1/2 max（回满）═════════════════════
    {
        ElfPet sp = make_pet(1, "己方", {kGuangYou, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300, kMaxHp);
        tune(sp, 100, true);
        ElfPet fp_hit = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 720000, kMaxHp);
        tune(fp_hit, 100, false);
        ScenarioBattle b(make_party(sp), make_party(fp_hit));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手命中
        b.drive();
        const int taken1 = kMaxHp - ctx.getPet(0).hp;
        CHECK(taken1 >= 205632 && taken1 <= 241920,
              "T2470.a 前置：R1 承伤带留缺口（实测 " + std::to_string(taken1) + "）");
        // R2：对手 acc=0 必 miss（先改命中率再提交——submit 即整回合结算；
        // ⚠️ 双方操作都要提交，缺一方回合不会推进）
        for (Skills& s : ctx.getPet(1).skills) {
            s.accuracy = 0;
            s.must_hit = 0;
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                 "T2470.a 177：MISS 后回合末回满 1/2（实测 hp " +
                     std::to_string(ctx.getPet(0).hp) + "）");
    }
    // ══ T2470.b 961：反转成功 → 下 2 回合免攻伤害；负例不挂窗 ════════════════
    {
        ElfPet sp = make_pet(1, "己方", {kSiJi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300, kMaxHp);
        tune(sp, 100, true);
        ElfPet fp = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 720000, kMaxHp);
        tune(fp, 100, false);
        ScenarioBattle b(make_party(sp), make_party(fp));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -1) == StatChangeResult::SUCCESS);   // 物攻-1（可反转）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);       // R1 载体（反转+武装）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.ability_levels[0][0], 1,
                 "T2470.b 961：降槽已反转为 +1（实测 " +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
        for (int r = 2; r <= 3; ++r) {
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手大刀
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp - hp0, 0,
                     "T2470.b 961：R" + std::to_string(r) + " 免疫攻击伤害（实测掉 " +
                         std::to_string(hp0 - ctx.getPet(0).hp) + "）");
        }
        const int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);       // R4 窗过期
        b.drive();
        const int taken = hp0 - ctx.getPet(0).hp;
        CHECK(taken >= 205632 && taken <= 241920,
              "T2470.b 961：R4 窗过期吃满带（实测 " + std::to_string(taken) + "）");
    }
    // ══ T2470.b- 961 负例：无降槽 → 反转失败不挂窗 ═══════════════════════════
    {
        ElfPet sp = make_pet(1, "己方", {kSiJi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300, kMaxHp);
        tune(sp, 100, true);
        ElfPet fp = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 720000, kMaxHp);
        tune(fp, 100, false);
        ScenarioBattle b(make_party(sp), make_party(fp));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int taken = hp0 - ctx.getPet(0).hp;
        CHECK(taken >= 205632 && taken <= 241920,
              "T2470.b- 961：反转失败 R2 吃满带（实测 " + std::to_string(taken) + "）");
    }
    // ══ T2470.c 522：有异常 → 受伤-300；负例不减；R4 过期 ════════════════════
    {
        ElfPet sp = make_pet(1, "己方", {kFuRong, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300, kMaxHp);
        tune(sp, 100, true);
        ElfPet fp = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 900, kMaxHp);
        tune(fp, 100, false);
        ScenarioBattle b(make_party(sp), make_party(fp));
        b.start();
        BattleContext& ctx = b.ctx();
        (void)apply_anomaly(&ctx, 0, kPoison, /*duration_rounds=*/9, /*actor=*/1);   // 自方中毒
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体（武装窗）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp0 = ctx.getPet(0).hp;   // 毒 tick 自此每回合 -100000（200000/两回合）
        for (int r = 2; r <= 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打红伤 [257,302]
            b.drive();
            const int taken = hp0 - ctx.getPet(0).hp;   // = 10 万×(r-1) + 红伤-300
            CHECK(taken >= 100000 * (r - 1) + 257 - 300
                    && taken <= 100000 * (r - 1) + 302 - 300,
                  "T2470.c 522：R" + std::to_string(r) + " 毒tick+减伤后恰 ["
                      + std::to_string(100000 * (r - 1) - 43) + ","
                      + std::to_string(100000 * (r - 1) + 2) + "]（实测 " +
                      std::to_string(taken) + "）");
        }
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);       // R4 过期
        b.drive();
        const int taken4 = hp0 - ctx.getPet(0).hp;   // 30 万毒 + 红伤带（无减伤）
        CHECK(taken4 >= 300000 + 257 && taken4 <= 300000 + 302,
              "T2470.c 522：R4 窗过期吃满带（实测 " + std::to_string(taken4) + "）");
    }
    // ══ T2470.c- 522 负例：无异常 → 不减 ═════════════════════════════════════
    {
        ElfPet sp = make_pet(1, "己方", {kFuRong, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300, 300, kMaxHp);
        tune(sp, 100, true);
        ElfPet fp = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100, 900, kMaxHp);
        tune(fp, 100, false);
        ScenarioBattle b(make_party(sp), make_party(fp));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int hp0 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int taken = hp0 - ctx.getPet(0).hp;
        CHECK(taken >= 257 && taken <= 302,
              "T2470.c- 522：无异常不减（实测 " + std::to_string(taken) + "）");
    }

    std::cout << "scenario_2470 PASS (J152 MISS/反转/减伤杂门 177/961/522)" << std::endl;
    return 0;
}
