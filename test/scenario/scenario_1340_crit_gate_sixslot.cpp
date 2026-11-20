// 场景 1340 —— J48 致命门六槽族（732，moves_lib/j_crit_gate_sixslot.cpp）
//（2026-10-02，worktree ../agents/j48；号段 1340-1349 → T1340 一块）
//
// 断言口径（32430 噬梦曲 [1,1,1,1,1,1] 全+1）：
//   - 未暴击 → 六槽各 +1（level_sum +6 精确等式，逐槽断言）；
//   - 必暴对照（set_crit_sixteenths 16/16）→ 不触发（sum 不变）。
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
constexpr int kShuiMeng = 32430;   // 噬梦曲 [732]|[1,1,1,1,1,1] 特150（全+1）
constexpr int kPaiDa   = 10004;    // 拍打（填充）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "致命门", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1340 732 未暴击→全+1（32430 [1,1,1,1,1,1]）══
    {
        {   // (a) 默认 0 暴击率 → 未暴击 → 六槽各 +1
            ScenarioBattle b(make_party(self_pet({kShuiMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const char* names = "攻防特攻特防速命中";
            for (int i = 0; i < 6; ++i) {
                CHECK_EQ(ctx.ability_levels[0][i], 1,
                         "T1340.a 732：未暴击 → 槽 " + std::to_string(i) + "（" +
                             std::string(1, names[i]) + "）+1");
            }
        }
        {   // (b) 必暴对照（16/16）→ 打出致命一击 → 不触发
            ScenarioBattle b(make_party(self_pet({kShuiMeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            set_crit_sixteenths(ctx.getPet(0), 16);   // 必暴
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.crit_happened[0], "T1340.b 前置：必暴已打出");
            int sum = 0;
            for (int i = 0; i < 6; ++i) {
                sum += ctx.ability_levels[0][i];
            }
            CHECK_EQ(sum, 0, "T1340.b 732：暴击过 → 六槽不动");
        }
        std::cout << "  [T1340] 732 致命门六槽+必暴对照 通过\n";
    }

    std::cout << "scenario_1340 PASS (J48 致命门六槽族)" << std::endl;
    return 0;
}
