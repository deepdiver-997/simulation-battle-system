// 场景 2270 —— J132 命中后窗族（1028，moves_lib/j_hit_cond_window2.cpp）
//（2026-10-04，worktree ../agents/j132；号段 2270-2279）
//
// 断言口径（星光·压迫 28739 [3,-2,0,-2,0,-2,-2,3] 属技：语义 攻/特攻/速/命中 各-2，
//           引擎槽 0/1/4/5；3 回合窗）：
//   - T2270 正常逐跳：R1 首跳 -2；R2/R3 tick 各再 -2（每回合重复施加，累计至 -6 地板）；
//     R4 窗过期不再变。
//   - T2271 兜底（AT_FLOOR 档）：T2270 战斗 R4 重施 → 四槽全在 -6 地板 → 零 SUCCESS →
//     对手最大体力 -33%（100/3 整数直除；断言变小区间）。
//   - T2272 兜底（IMMUNE 档）：对手带 harness 免弱魂 → 首跳全 IMMUNE → 最大体力直接缩。
//   - T2273 命中门：accuracy 压到 1 制造 miss → 对手无任何变化。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <entities/numerical-properties.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kXingGuang = 28739; // 星光·压迫 [1028]|[3,-2,0,-2,0,-2,-2,3]（属）

void accuracy_set(ElfPet& p, int accuracy, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = accuracy;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int accuracy = 100, bool must_hit = true) {
    ElfPet p = make_pet(1, "命中窗", skills, /*speed=*/300, /*atk=*/1, 5000);
    accuracy_set(p, accuracy, must_hit);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, bool drop_immune = false) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 999999,
                        drop_immune ? make_stat_drop_immunity_soul_mark() : SoulMark{});
    accuracy_set(p, 100, /*must_hit=*/false);
    return p;
}

int foe_negative_count(const BattleContext& ctx) {
    int n = 0;
    for (int i = 0; i < 6; ++i) {
        if (ctx.ability_levels[1][i] < 0) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2270 + T2271 逐跳累计 + AT_FLOOR 兜底（同一场战斗）══
    {
        ScenarioBattle b(make_party(self_pet({kXingGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int max_before = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        // R1 首跳
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(foe_negative_count(ctx), 4, "T2270 R1 首跳：四槽转负");
        CHECK_EQ(ctx.ability_levels[1][0], -2, "T2270 R1：攻 -2（语义0→引擎0）");
        CHECK_EQ(ctx.ability_levels[1][1], -2, "T2270 R1：特攻 -2（语义2→引擎1）");
        // R2/R3 tick 逐跳累计
        for (int r = 2; r <= 3; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -2 * r,
                     "T2270 R" + std::to_string(r) + "：攻累计 -" + std::to_string(2 * r));
        }
        // R4 窗过期 + 重施 → AT_FLOOR 兜底
        const int max_at_r4 = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 重施
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.ability_levels[1][0], -6, "T2270 R4：窗过期、重施首跳仍在地板 -6");
        const int max_after = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        CHECK(max_after < max_at_r4 && max_at_r4 == max_before,
              "T2271 AT_FLOOR 兜底：最大体力缩水（" + std::to_string(max_at_r4) + "→" +
                  std::to_string(max_after) + "）");
        std::cout << "  [T2270/T2271] 逐跳累计 + AT_FLOOR 兜底 通过\n";
    }

    // ══ T2272 IMMUNE 兜底（免弱魂）══
    {
        ScenarioBattle b(make_party(self_pet({kXingGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*drop_immune=*/true)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int max_before = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(foe_negative_count(ctx), 0, "T2272 免弱：槽位不动");
        const int max_after = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        CHECK(max_after < max_before,
              "T2272 IMMUNE 兜底：最大体力缩水（" + std::to_string(max_before) + "→" +
                  std::to_string(max_after) + "）");
        std::cout << "  [T2272] IMMUNE 兜底 通过\n";
    }

    // ══ T2273 命中门（miss 无事发生）══
    {
        ScenarioBattle b(make_party(self_pet({kXingGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                             /*accuracy=*/1, /*must_hit=*/false)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        const int max_before = ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP];
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(foe_negative_count(ctx), 0, "T2273 miss：无槽位变化");
        CHECK_EQ(ctx.getPet(1).numericalBase[NumericalPropertyIndex::HP], max_before,
                 "T2273 miss：最大体力不变");
        std::cout << "  [T2273] 命中门 通过\n";
    }

    std::cout << "scenario_2270 ALL PASS" << std::endl;
    return 0;
}
