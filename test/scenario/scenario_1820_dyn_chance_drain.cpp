// 场景 1820 —— J93 动态概率/后手吸取族（2 宏，moves_lib/j_dyn_chance_drain.cpp）
//（2026-10-03，worktree ../agents/j93；号段 1820-1829 → T1820-T1821 两块）
//
// 断言口径：
//   - 1365 动态概率的确定性档：载体 狂战荡风锤 [80,6,20,5]（base 80、步长 20、
//     增量 5）→ 预置自身 hp=15%（已损 85% → floor(85/20)=4 档 → chance=80+20=100
//     clamp）→ 怕(6) 确定性落地；对照 = 对手挂 ANOMALY 免疫票 → 任意概率被挡；
//   - 1189 后手门 = 速度倒置对照：后手吸 150（自 +150 精确——fixed 无浮动）、
//     先手不吸；对手全程 NONE；
//   - 载体兄弟（867 已验良性 / 1366 哑；1189 纯链 复苏之火）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kKuangZhan = 35860; // 狂战荡风锤 [867,1365,1366]|[2,80,6,20,5,…]：物160
constexpr int kFuSu   = 31082;  // 复苏之火 [1189]|[300]：特130（后手吸取载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int speed) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1820 1365 动态概率（狂战荡风锤 [80,6,20,5]）══
    {
        // a) 已损 85% → chance=100 → 怕(6) 确定性落地
        {
            ScenarioBattle b(
                make_party(mk_self({kKuangZhan, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 750;                   // 已损 85% → 4 档 → 80+20=100
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, 6),
                  "T1820.a 1365：已损 85% → chance=100 → 怕(6) 落地");
        }
        // b) 对照：对手 ANOMALY 免疫 → 被挡
        {
            ScenarioBattle b(
                make_party(mk_self({kKuangZhan, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 750;
            ctx.grant_immunity(1, ImmunityType::ANOMALY, coverage_all(),
                               /*anomaly_mask=*/0, /*duration_rounds=*/3,
                               /*source_id=*/93, /*soul_immunity=*/false,
                               EffectScope::ON_STAGE, /*counts=*/0,
                               /*source_effect_id=*/93);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, 6),
                  "T1820.b 1365 对照：免疫被挡（自身无怕）");
        }
        std::cout << "  [T1820] 1365 动态概率 通过\n";
    }

    // ══ T1821 1189 后手吸取 150（复苏之火 [150]）══
    {
        // a) 后手：吸 300（自 +300 精确）
        {
            ScenarioBattle b(
                make_party(mk_self({kFuSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;                  // 预置缺口（防回吸钳制）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4300,
                     "T1821.a 1189：后手吸取 300（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 先手对照：不吸
        {
            ScenarioBattle b(
                make_party(mk_self({kFuSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 4000,
                     "T1821.b 1189 对照：先手门关不吸（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1821] 1189 后手吸取 通过\n";
    }

    std::cout << "scenario_1820 PASS (J93 动态概率/后手吸取族 1365/1189)" << std::endl;
    return 0;
}
