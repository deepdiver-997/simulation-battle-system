// 场景 879 —— 回合上限内建（D3，2026-10-01《终局与死切状态机改造单》）
//
// 模型：进入新回合（ROUND_START 入口）检查 roundCount >= max_rounds（ctx 新字段，
// 0 = 无限）→ 跳 FINISHED + on_finish。回合计数语义：round N 的 ROUND_START 时
// roundCount == N-1（advanceRound 在 ROUND_COMPLETION）→ 判据在第 max_rounds+1 回合
// 入口触发，恰好打满 max_rounds 个整回合。
//
// 断言组：
//   A 上限=2：两个整回合打满后，第 3 回合入口拦截 → FINISHED + 旗 + on_finish 一次，
//     roundCount 停在 2（终局回合的 ROUND_START 桶不执行——位置在重置/时点桶之前）。
//   B 对照（上限=0）：同配置 3 个回合打完仍在选择期、未终局——防"永远会终局"假绿。
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kGaoSu = 20017;   // 高速移动：属性技（无伤害噪声）

// 高血低攻桩：回合内不可能出现死亡，纯耗回合。
ElfPet tank_pet(int id) {
    return scenario::make_pet(id, "耐久桩", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              /*speed=*/100, /*atk=*/1, /*hp=*/1000000);
}

void skip_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ A 上限=2：第 3 回合入口拦截 ══════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(tank_pet(1)),
                         scenario::make_party(tank_pet(2)));
        BattleContext& ctx = b.ctx();
        ctx.max_rounds = 2;
        int finish_calls = 0;
        ctx.on_finish = [&](BattleContext& c) {
            ++finish_calls;
            CHECK(c.finish_flag, "A on_finish 回调时旗已立");
        };
        b.start();
        for (int round = 0; round < 8 && ctx.currentState != State::FINISHED; ++round) {
            skip_round(b);
        }
        CHECK_EQ(static_cast<int>(ctx.currentState), static_cast<int>(State::FINISHED),
                 "A 回合上限触发 FINISHED");
        CHECK(ctx.finish_flag, "A 结束标志已立");
        CHECK_EQ(finish_calls, 1, "A on_finish 恰好一次");
        CHECK_EQ(ctx.roundCount, 2, "A 恰好打满 2 个整回合（roundCount 停在 2）");
        // 双方主力都还活着：终局来自上限，不是全灭。
        CHECK(ctx.seerRobot[0].elfPets[ctx.on_stage[0]].hp > 0
                  && ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp > 0,
              "A 双方场上存活（终局来自回合上限而非全灭）");
        std::cout << "  [A] 上限=2：打满两整回合、第 3 回合入口 FINISH ✓" << std::endl;
    }

    // ══ B 对照：上限=0（无限）不终局 ═════════════════════════════════════════
    {
        ScenarioBattle b(scenario::make_party(tank_pet(1)),
                         scenario::make_party(tank_pet(2)));
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.max_rounds, 0, "B max_rounds 默认 0（无限）");
        b.start();
        for (int round = 0; round < 3; ++round) {
            skip_round(b);
        }
        CHECK(ctx.currentState != State::FINISHED, "B 无上限时 3 回合后仍在进行");
        CHECK(!ctx.finish_flag, "B 无上限时不立结束标志");
        std::cout << "  [B] 对照：上限=0 打 3 回合不终局 ✓" << std::endl;
    }

    std::cout << "scenario_879 PASS (回合上限内建：ROUND_START 入口检查 → FINISH + on_finish)"
              << std::endl;
    return 0;
}
