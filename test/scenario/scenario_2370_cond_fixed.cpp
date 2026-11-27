// 场景 2370 —— J144 条件固伤族（515/685/1027，moves_lib/j_cond_fixed.cpp）
//（2026-10-04，worktree ../agents/j144；号段 2370-2379 → T2370 一块五例）
//
// 断言口径（红伤带 = 0.84×P×A/D，A=300/D=100；make_pet 默认无任何升降槽）：
//   - a 685 有提升：速槽+1（任一正槽开门）→ 红伤 [321,378] + 附 90；
//   - b 685 无提升：红伤带内无附加；
//   - c 515 有降：物攻槽-1（载体是特殊技吃特攻槽，红伤不带）→ 红伤 [278,328]
//     + 附 150，且物攻槽回 0（clear_stat_drops 验证）；
//   - d 515 无降：红伤带内无附加；
//   - e 1027 窗：鬼哭神嚎 [3,300] 属性技——R1 恰 300；R2/R3 自方拍打出手（实测
//     NONE 行动不走 AFTER_ACTION 桶，窗 tick 不跑——「每回合判」挂在行动上），
//     每回合拍打红伤 [85,100] + 300；R4 过期仅红伤。链上 48/775 均休眠良性。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kDuanBi  = 16775;  // 断壁流沙拳 [685]|[90]：物150 纯链
constexpr int kTianMo  = 15714;  // 天魔极光闪 [515]|[150]：特130 纯链
constexpr int kGuiKu   = 25119;  // 鬼哭神嚎 [48,775,1027]|[3,3,200,3,300]：属性技（窗 R1-R3）
constexpr int kPaiDa   = 10004;  // 拍打：填充位

constexpr int kMaxHp = 800000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, kMaxHp);
    accuracy_100(p, true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    accuracy_100(p, false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2370.a 685 有提升 → 附 90（断壁流沙拳）══════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kDuanBi)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 4, +1) == StatChangeResult::SUCCESS);   // 速+1（正槽开门）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 411 && foe_loss <= 468,
              "T2370.a 685：红伤 [321,378] + 附 90（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2370.b 685 无提升 → 不附 ════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kDuanBi)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 321 && foe_loss <= 378,
              "T2370.b 685：无提升只吃红伤带（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2370.c 515 有降 → 附 150 + 解除降（天魔极光闪，特殊技）═══════════════
    {
        ScenarioBattle b(make_party(self_pet(kTianMo)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, 0, -1) == StatChangeResult::SUCCESS);   // 物攻-1（负槽开门）
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 428 && foe_loss <= 478,
              "T2370.c 515：红伤 [278,328] + 附 150（实测 " + std::to_string(foe_loss) + "）");
        CHECK_EQ(ctx.ability_levels[0][0], 0,
                 "T2370.c 515：物攻槽已解除（实测 " +
                     std::to_string(ctx.ability_levels[0][0]) + "）");
    }
    // ══ T2370.d 515 无降 → 不附 ══════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kTianMo)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK(foe_loss >= 278 && foe_loss <= 328,
              "T2370.d 515：无降只吃红伤带（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2370.e 1027 窗内无提升每回合附 300、过期归零（鬼哭神嚎，属性技零红伤）══
    {
        ScenarioBattle b(make_party(self_pet(kGuiKu)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装+tick1
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        int hp1 = ctx.getPet(1).hp;
        CHECK_EQ(kMaxHp - hp1, 300, "T2370.e 1027：R1 无提升附 300");
        for (int r = 2; r <= 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2/R3 自方拍打（有行动才判）
            b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
            b.drive();
            const int loss = hp1 - ctx.getPet(1).hp;
            const int red = ctx.ws.raw_attack_damage[0];
            CHECK(red > 0, "T2370.e 前置：拍打造成红伤");
            CHECK_EQ(loss, red + 300, "T2370.e 1027：窗口内红伤之外精确附加 300");
            hp1 = ctx.getPet(1).hp;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);       // R4 窗过期（仅拍打红伤）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int r4 = hp1 - ctx.getPet(1).hp;
        CHECK(ctx.ws.raw_attack_damage[0] > 0, "T2370.e 前置：R4 拍打造成红伤");
        CHECK_EQ(r4, ctx.ws.raw_attack_damage[0], "T2370.e 1027：R4 窗过期仅红伤");
    }

    std::cout << "scenario_2370 PASS (J144 条件固伤族 515/685/1027)" << std::endl;
    return 0;
}
