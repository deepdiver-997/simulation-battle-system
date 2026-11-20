// 场景 1070 —— J22 属性技响应/攻击技窗族（718/592，moves_lib/j_attr_response_window.cpp）
//（2026-10-02，worktree ../agents/j22；号段 1070-1079 → T1070-T1071 两块）
//
// 断言口径：
//   - 718 必暴断言读 **resolvedDamage.isCrit**（R2 攻击后的管线终态；atk=1 消伤害噪声，
//     附带的 1088 在 atk=1 时休眠——"伤害>300"门槛够不着）；触发器=对手真用属性技
//     （高速移动 20017），对照=对手用攻击技不触发；
//   - 592 用 chance=100 载体（毒灵妖魄 [3,100,5]）→ 冻伤落地确定性断言；攻击技门
//     对照=同回合改用属性技（592 载体自身是属性技，R2 用拍打/另一属性技分别探两半）。
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
constexpr int kPaiDa  = 10004;   // 拍打：物40（crit 探针/攻击）
constexpr int kZuiE   = 38081;   // 罪恶类列 [718]|[2,1]（+1088 atk=1 休眠）物150
constexpr int kDuLing = 25747;   // 毒灵妖魄 [48,693,164,592]|[4,…,3,100,5] 属（592 100%冻伤）
constexpr int kGaoSu  = 20017;   // 高速移动（effect 4 属性技，对手触发器）

constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);   // 5

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "窗族", skills, speed, atk, hp);
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

    // ══ T1070 718 属性技响应→必暴（38081 [2,1]：2 回合窗、下 1 回合必暴）══
    {
        {   // (a) R1 对手用属性技 → R2 我攻击必暴；R3 窗口过期不暴
            ScenarioBattle b(make_party(self_pet({kPaiDa, kZuiE, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kGaoSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 罪恶类列
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手属性技（触发）
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.resolvedDamage.isCrit, "T1070.a 718：对手用属性技 → 下回合必暴");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.resolvedDamage.isCrit, "T1070.a 718：必暴窗（下1回合）已过");
        }
        {   // (b) 对照：R1 对手用攻击技（不触发）
            ScenarioBattle b(make_party(self_pet({kPaiDa, kZuiE, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 罪恶类列
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手攻击技
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.resolvedDamage.isCrit, "T1070.b 718 对照：对手用攻击技不触发必暴");
        }
        std::cout << "  [T1070] 718 属性技响应必暴+对照 通过\n";
    }

    // ══ T1071 592 攻击技窗异常（25747 [3,100,5]：下 3 回合、100% 冻伤）══
    {
        {   // (a) R2 用攻击技 → 冻伤落地
            ScenarioBattle b(make_party(self_pet({kPaiDa, kDuLing, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 毒灵妖魄（开窗）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
                  "T1071.a 前置：R1 属性技不触发（592 门=攻击技）");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
                  "T1071.a 592：下回合攻击技 100% 令对手冻伤");
        }
        {   // (b) R2 用属性技 → 门关不施加
            ScenarioBattle b(make_party(self_pet({kDuLing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 毒灵妖魄（开窗）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 重用毒灵妖魄（属性技）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
                  "T1071.b 592 门槛：属性技不触发");
        }
        std::cout << "  [T1071] 592 攻击技窗异常+门 通过\n";
    }

    std::cout << "scenario_1070 PASS (J22 属性技响应/攻击技窗族)" << std::endl;
    return 0;
}
