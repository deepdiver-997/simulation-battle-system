// 场景 2410 —— J146 暴击递增族（1 宏，moves_lib/j_crit_ramp.cpp）
//（2026-10-04，worktree ../agents/j146；号段 2410-2419 → T2410 一块）
//
// 断言口径（**审计槽 F9 782**：申报概率×100 落 plugin_storage 990441100+side，
// 无随机精确断言——暴击掷骰本身随机，行为面不断言）：
//   - 载体 13655 天翔龙炎击 [10,30] 纯链（步长 10、上限 30）连打 5 回合：
//     R1 无增益（audit=0）→ R2 1000 → R3 2000 → R4 3000 → R5 3000（封顶，
//     N×10=40 被 cap 30 截断）；
//   - 附带行为侧证：R5 槽内 crit_rate_add=30>0（ws 视图通道已吃到）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kTianXiang = 13655;  // 天翔龙炎击 [441]|[10,30]：物150 纯链
constexpr int kPaiDa     = 10004;
constexpr int kAuditBase = 990441100;  // 审计槽基址（实现头同款）
constexpr int kMaxHp     = 800000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kTianXiang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

int audit_of(const BattleContext& ctx, int side) {
    const auto it = ctx.plugin_storage.find(kAuditBase + side);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* v = std::any_cast<int>(&it->second);
    return v ? *v : 0;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    const int expect[5] = {0, 1000, 2000, 3000, 3000};   // R1 未写入=0；R5 封顶
    for (int round = 1; round <= 5; ++round) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "R%d", round);
        CHECK_EQ(audit_of(ctx, 0), expect[round - 1],
                 std::string("T2410 441：") + buf + " 申报概率审计槽（期望 " +
                     std::to_string(expect[round - 1]) + "）");
    }
    CHECK(ctx.ws.crit_rate_add[0] > 0.0f,
          "T2410 441：ws 加算通道已吃到（封顶 30 个百分点）");

    std::cout << "scenario_2410 PASS (J146 暴击递增族 441)" << std::endl;
    return 0;
}
