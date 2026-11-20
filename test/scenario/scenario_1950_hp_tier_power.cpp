// 场景 1950 —— J102 血量阈值变威力族（1 宏，moves_lib/j_hp_tier_power.cpp）
//（2026-10-03，worktree ../agents/j102；号段 1950-1959 → T1950 三块）
//
// 断言口径：
//   - 载体祭魂锋刃 31254 [889,1252]|[6,2,2,3,3]：物85 prio3（ prio 不进断言面）；
//     1252=[2,2,3,3]：hp<1/3 → ×3（深档优先），hp<1/2 → ×2，健康 → 原倍；
//   - 物85·atk200 基带 [123,144]（0.84×2×85+2=144.8 ×[0.851,1.0]）；
//     ×2 带 [246,288]、×3 带 [369,432]——三带互不重叠；
//   - hp 预设点位：full=99999（无档）、max/3=33333（浅档：33333<49999 ✓ 且
//     33333<33333 ✗）、max/5=19999（深档）；
//   - 链上 889=消对手回合类（空场消除失败=良性，已注册 selection_seal）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打
constexpr int kJiHun = 31254;   // 祭魂锋刃[889,1252]|[6,2,2,3,3]：物85（血量阈值变威力）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, 200, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 跑一回合：自身 hp 预设为 hp_set 后用祭魂锋刃出手，返回对手承伤。
int round_damage(int hp_set) {
    ScenarioBattle b(make_party(self_pet({kJiHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                     make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
    b.start();
    BattleContext& ctx = b.ctx();
    if (hp_set > 0) {
        ctx.getPet(0).hp = hp_set;
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    return kMaxHp - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1950 1252 血量阈值变威力（祭魂锋刃 [2,2,3,3]）══
    {
        const int ctrl = round_damage(0);            // 满血：无倍率
        const int shallow = round_damage(kMaxHp / 3); // 1/3：浅档 ×2
        const int deep = round_damage(kMaxHp / 5);    // 1/5：深档 ×3
        CHECK(ctrl >= 123 && ctrl <= 144,
              "T1950 1252：满血基带（实测 " + std::to_string(ctrl) + "，带 [123,144]）");
        CHECK(shallow >= 246 && shallow <= 288,
              "T1950 1252：浅档 ×2（实测 " + std::to_string(shallow) + "，带 [246,288]）");
        CHECK(deep >= 369 && deep <= 432,
              "T1950 1252：深档 ×3（实测 " + std::to_string(deep) + "，带 [369,432]）");
        std::cout << "  [T1950] 1252 血量阈值变威力 通过 (基带 " << ctrl << " / ×2 " << shallow
                  << " / ×3 " << deep << ")\n";
    }

    std::cout << "scenario_1950 PASS (J102 血量阈值变威力族 1252)" << std::endl;
    return 0;
}
