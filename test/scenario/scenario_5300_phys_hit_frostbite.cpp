// 场景 5300 —— J205 受物理攻击概率冻伤窗族（92，moves_lib/j_phys_hit_frostbite.cpp)
//（2026-10-22，号段 5300-5309）
//
// 断言口径：
//   - 92（冰封之铠 23022 链 [4,4,92]，92 段切片 [5,100] → 5 回合 100%）：
//     · (a) 开窗后对手**物理**攻击（拍打 10004）→ 100% 冻伤(5)落对手；审计=1001。
//     · (b) 对手**特殊**攻击（水枪 10013）→ 物理门不成立：无冻伤、审计缺席
//       （绝对量断言，不写"与对照相等"）。
//     · (c) 窗过期：R1 开窗后 R2-R5 无攻击，R6 对手物理攻击 → 不再触发（审计缺席）。
//   - 冻伤判定读 has_active_abnormal_status(1, Frostbite)。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物 40
constexpr int kShuiQiang = 10013; // 水枪：特 40
constexpr int kBingKai  = 23022;  // [4,4,92] → 92 段 [5,100]
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);
constexpr int kAuditKey = 990092100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kBingKai, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int attack_skill) {
    ElfPet p = make_pet(2, "对手", {attack_skill, attack_skill, attack_skill, attack_skill,
                                    attack_skill},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int audit(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5300 92 受物理攻击 → 概率冻伤（100% 载体确定性）══
    {
        {   // (a) 物理攻击 → 冻伤
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe(kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
                  "T5300.a 92：物理攻击 → 对手冻伤");
            CHECK_EQ(audit(ctx), 1001, "T5300.a 92：100% 申报且落地（审计 " +
                                           std::to_string(audit(ctx)) + "）");
        }
        {   // (b) 特殊攻击 → 物理门不成立
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe(kShuiQiang)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
                  "T5300.b 92：特殊攻击 → 不触发冻伤");
            CHECK_EQ(audit(ctx), -1, "T5300.b 92 对照：审计缺席");
        }
        {   // (c) 窗过期：R6 物理攻击不再触发
            ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe(kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1 开窗（对手不出手）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // R2-R5 空过
            for (int r = 2; r <= 5; ++r) {
                b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
                b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
                b.drive();
            }
            CHECK_EQ(audit(ctx), -1, "T5300.c 前置：窗内无物理攻击 → 未触发");
            // R6 窗已过期（5 回合窗：R1-R5）
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
                  "T5300.c 92：窗过期后物理攻击不再冻伤");
            CHECK_EQ(audit(ctx), -1, "T5300.c 92：过期后审计仍缺席");
        }
        std::cout << "  [T5300] 92 物理攻击冻伤 + 特殊门 + 窗过期 通过\n";
    }

    std::cout << "scenario_5300 PASS (J205 受物理攻击概率冻伤窗族)" << std::endl;
    return 0;
}
