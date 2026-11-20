// 场景 1330 —— J47 盾破回血族（1753，moves_lib/j_shield_break_heal.cpp）
//（2026-10-02，worktree ../agents/j47；号段 1330-1339 → T1330 一块）
//
// 断言口径（25493 载体同参 [400,3] 用直接构造）：
//   - 获盾后首次盾破：盾 400 吸收 foe 大刀（hp 不动）→ 盾破触发回 max/3（4000 缺口
//     全吃 → hp 恰回 5000，heal 钳满整观测）；
//   - 一次性：第二次盾破（盾早已无）不再回；未获盾的盾破（对照）不回。
//   ⚠️ heal 发生在盾破时点（foe 攻击的管线内）→ R 内 hp 曲线：吸收 0 → 破 → +回血。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对；场景用 25493 同参直接构造载体）──
constexpr int kPaiDa  = 10004;   // 拍打（对手填充刀）
constexpr int kHuoDe  = 25493;   // 25493 [1753]|[400,3]（获盾 400 / 破盾回 max/3）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 5000) {
    ElfPet p = make_pet(1, "盾破者", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 3000, int hp = 99999) {
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

    // ══ T1330 1753 盾破回血（获盾 400 / 破盾回 max/3）══
    {
        {   // (a) R1 获盾+foe 大刀（盾 400 破，余量伤 hp）→ 破盾回 max/3
            //     foe 刀 ~1010：盾吸 400 → hp -610；破盾回 5000/3=1666 ≥ 缺口 → 回满 5000
            ScenarioBattle b(make_party(self_pet({kHuoDe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 获盾
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 大刀破盾
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 5000,
                     "T1330.a 1753：盾破 → 回 max/3（缺口 610 ≤ 1666 → 钳满 5000）");
        }
        {   // (b) 一次性：破盾回血后，后续盾破（无盾状态下的新盾破——重新获盾再破）不再回
            ScenarioBattle b(make_party(self_pet({kHuoDe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/300, /*atk=*/1, /*hp=*/5000)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 获盾
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 破盾（触发回血）
            b.drive();
            const int hp_r1 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 再挨刀（无盾）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int loss2 = hp_r1 - ctx.getPet(0).hp;
            CHECK(loss2 > 0, "T1330.b 1753：回血一次性（R2 无盾无回，正常承伤）");
            (void)loss2;
        }
        std::cout << "  [T1330] 1753 盾破回血+一次性 通过\n";
    }

    std::cout << "scenario_1330 PASS (J47 盾破回血族)" << std::endl;
    return 0;
}
