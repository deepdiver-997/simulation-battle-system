// 场景 2430 —— J148 命中概率递增族（1 宏，moves_lib/j_chance_ramp.cpp）
//（2026-10-04，worktree ../agents/j148；号段 2430-2439 → T2430 一块）
//
// 断言口径（**审计槽 F9 782**：申报概率×100 落 plugin_storage 990181100+side）：
//   - 载体 16963 明王焚天照 [40,2,20,100] 纯链（初值 40、步长 20、增幅封顶 +100
//     → 100% 封顶）连打 4 回合：审计槽 4000/6000/8000/10000 精确梯度；
//   - R4 申报 100% → 烧伤（码 2）确定性落地（行为侧证；R1-R3 落不落地不依赖）；
//   - 中毒/烧伤 tick 与红伤不影响审计断言（foe hp 不断言）。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kMingWang = 16963;  // 明王焚天照 [181]|[40,2,20,100]：物150 纯链
constexpr int kPaiDa   = 10004;
constexpr int kAuditBase = 990181100;
constexpr int kBurn    = static_cast<int>(AbnormalStatusId::Burn);  // 2
constexpr int kMaxHp   = 800000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kMingWang, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

    const int expect[4] = {4000, 6000, 8000, 10000};   // 40/60/80/100 封顶
    for (int round = 1; round <= 4; ++round) {
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        char buf[16];
        std::snprintf(buf, sizeof(buf), "R%d", round);
        CHECK_EQ(audit_of(ctx, 0), expect[round - 1],
                 std::string("T2430 181：") + buf + " 申报概率审计槽（期望 " +
                     std::to_string(expect[round - 1]) + "）");
    }
    CHECK(ctx.has_active_abnormal_status(1, kBurn),
          "T2430 181：R4 申报 100% 烧伤确定性落地");

    std::cout << "scenario_2430 PASS (J148 命中概率递增族 181)" << std::endl;
    return 0;
}
