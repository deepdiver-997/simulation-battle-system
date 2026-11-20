// 场景 1520 —— J65 状态门固伤族（141/657/684，moves_lib/j_status_gate_fixed.cpp）
//（2026-10-03，worktree ../agents/j65；号段 1520-1529 → T1520-T1522 三块）
//
// 断言口径（冻伤/异常 tick 污染 hp 差分 → 全程用粉伤 FINAL 探针数笔数，T1491 同款）：
//   - 141 冻伤门（15492 [100] 特150）：对手冻伤 → 恰 1 笔 ==100；对照无冻伤 → 0 笔。
//   - 657 解除成功门（18255 [0,100] 特80，{0}=0 麻痹）：对手麻痹 → 恰 1 笔 ==100 且
//     回合后麻痹已解除；对照无麻痹 → 0 笔（未处于状态不解不附）。
//   - 684 任一负槽门（16755 [200] 物80）：仅对手有负槽 → 恰 1 笔 ==200 且自身无负槽
//     不动对手槽；仅自身有负槽 → 同样触发且自身负槽清零；对照双方无负槽 → 0 笔。
//   atk=1 消红噪声。
#include <iostream>
#include <vector>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打（填充刀）
constexpr int kXieHua  = 15492;   // 邪化冰凝光 [141]|[100] 特150（冻伤门 +100）
constexpr int kHuiMie  = 18255;   // 毁灭次元 [657]|[0,100] 特80（解麻痹成功 +100）
constexpr int kPoDan   = 16755;   // 破胆拳 [684]|[200] 物80（任一负槽 +200+自清）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "固伤族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99998) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 粉伤 FINAL 探针（T1491 同款）：收集落在对手身上的每笔粉伤终值。
void install_pink_probe(BattleContext& ctx, std::vector<int>& pinks) {
    ctx.register_pink_damage_effect(
        PinkDamagePhase::FINAL, 1, [&pinks](BattleContext* c, int) {
            const PinkDamageResolved& p = c->resolvedPink;
            if (p.target == 1 && p.final > 0) {
                pinks.push_back(p.final);
            }
        });
}

int count_of(const std::vector<int>& v, int x) {
    int n = 0;
    for (const int e : v) {
        if (e == x) {
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

    // ══ T1520 141 冻伤门（15492 [100]）══
    {
        {   // (a) 对手冻伤 → 恰 1 笔 ==100
            ScenarioBattle b(make_party(self_pet({kXieHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Frostbite), 9, 0);
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 1,
                     "T1520.a 141：对手冻伤 → 恰 1 笔 +100（pinks=" +
                         std::to_string(pinks.size()) + " 笔）");
        }
        {   // (b) 对照：无冻伤 → 0 笔
            ScenarioBattle b(make_party(self_pet({kXieHua, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 0,
                     "T1520.b 141 门：无冻伤无附加（pinks=" + std::to_string(pinks.size()) +
                         " 笔）");
        }
        std::cout << "  [T1520] 141 冻伤门固伤 通过\n";
    }

    // ══ T1521 657 解除成功门（18255 [0,100]）══
    {
        {   // (a) 对手麻痹 → 恰 1 笔 ==100 且麻痹被解除
            ScenarioBattle b(make_party(self_pet({kHuiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)apply_anomaly(&ctx, 1, static_cast<int>(AbnormalStatusId::Paralysis), 9, 0);
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 1,
                     "T1521.a 657：解麻痹成功 → 恰 1 笔 +100（pinks=" +
                         std::to_string(pinks.size()) + " 笔）");
            CHECK(!ctx.has_active_abnormal_status(1,
                                                  static_cast<int>(AbnormalStatusId::Paralysis)),
                  "T1521.a 657：麻痹已解除");
        }
        {   // (b) 对照：无麻痹 → 0 笔（成功门槛）
            ScenarioBattle b(make_party(self_pet({kHuiMie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 100), 0,
                     "T1521.b 657 门：无状态不解不附（pinks=" + std::to_string(pinks.size()) +
                         " 笔）");
        }
        std::cout << "  [T1521] 657 解除成功门 通过\n";
    }

    // ══ T1522 684 任一负槽门 + 自清（16755 [200]）══
    {
        {   // (a) 仅对手有负槽 → 恰 1 笔 ==200；自身槽不动、对手负槽保留
            ScenarioBattle b(make_party(self_pet({kPoDan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 1, 0, -2) == StatChangeResult::SUCCESS);
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 200), 1,
                     "T1522.a 684：对手负槽 → 恰 1 笔 +200（pinks=" +
                         std::to_string(pinks.size()) + " 笔）");
            CHECK_EQ(ctx.ability_levels[1][0], -2, "T1522.a 对手负槽保留（只清自身）");
        }
        {   // (b) 仅自身有负槽 → 触发 + 自身负槽清零
            ScenarioBattle b(make_party(self_pet({kPoDan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(stat_change(&ctx, 0, 0, -3) == StatChangeResult::SUCCESS);
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 200), 1,
                     "T1522.b 684：自身负槽也触发 → 恰 1 笔 +200");
            CHECK_EQ(ctx.ability_levels[0][0], 0, "T1522.b 自身负槽已清零");
        }
        {   // (c) 对照：双方无负槽 → 0 笔
            ScenarioBattle b(make_party(self_pet({kPoDan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            std::vector<int> pinks;
            install_pink_probe(ctx, pinks);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_of(pinks, 200), 0,
                     "T1522.c 684 门：无负槽无附加（pinks=" + std::to_string(pinks.size()) +
                         " 笔）");
        }
        std::cout << "  [T1522] 684 任一负槽门+自清 通过\n";
    }

    std::cout << "scenario_1520 PASS (J65 状态门固伤族)" << std::endl;
    return 0;
}
