// 场景 2220 —— J128 门族三·异常（434/2158，moves_lib/j_anomaly_gates3.cpp）
//（2026-10-04，worktree ../agents/j128；号段 2220-2229）
//
// 断言口径：
//   - 434 提升门概率异常（冰冻之光 13913 [100,5]，100% 确定档）：
//       (a) R1 我高速移动（有提升）→ R2 施放 → 对手冻伤落地；
//       (b) 对照：无提升直接施放 → 不落（先短路后掷骰，门在 roll 前）。
//   - 2158 解除自身异常→成功令对手冰封（星光·惊涛骇浪 37386 [15]）：
//       (a) R1 直调 apply_anomaly 给我挂中毒 9 回合（⚠️ 冰封是控制类异常——被冻无法
//           行动，R2 出不了手，第一批用悬冰伏徒造冰封的路线作废）→ R2 我施放 →
//           我中毒被解除 + 对手冰封落地；
//       (b) 对照：我无异常时施放 → 对手不冰封（解除失败门）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;    // 拍打
constexpr int kGaoSu  = 20017;    // 高速移动（造提升）
constexpr int kBingDong = 13913;  // 冰冻之光 [434]|[100,5]（特80）
constexpr int kXingGuang = 37386; // 星光·惊涛骇浪 [2158]|[15]（特140）
constexpr int kXuanBing = 37262;  // 悬冰伏徒 [680]|[100,15,2]（100% 冰封，造异常）

constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);   // 15
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite); // 5 冻伤
constexpr int kPoison = static_cast<int>(AbnormalStatusId::Poison);       // 1 中毒（非控制）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "异常门", skills, /*speed=*/300, /*atk=*/1, 5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, 999999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2220 434 提升门概率异常 ══
    {
        {   // (a) 有提升 → 100% 冻伤落地
            ScenarioBattle b(make_party(self_pet({kGaoSu, kBingDong, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 高速移动
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // 冰冻之光
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
                  "T2220.a 434 有提升：100% 冻伤落地");
        }
        {   // (b) 无提升对照 → 不落
            ScenarioBattle b(make_party(self_pet({kBingDong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
                  "T2220.b 434 无提升对照：不落地");
        }
        std::cout << "  [T2220] 434 提升门+对照 通过\n";
    }

    // ══ T2221 2158 解除自身异常→令对手冰封 ══
    {
        {   // (a) 我中毒（直调、非控制类）→ 施放解除 + 对手冰封
            ScenarioBattle b(make_party(self_pet({kXingGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            REQUIRE(apply_anomaly(&ctx, 0, kPoison, /*duration=*/9) == ApplyAnomalyResult::SUCCESS);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 星光·惊涛骇浪
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(0, kPoison), "T2221.a 我方中毒被解除");
            CHECK(ctx.has_active_abnormal_status(1, kIcebound),
                  "T2221.a 解除成功：对手冰封落地");
        }
        {   // (b) 我无异常对照 → 不落
            ScenarioBattle b(make_party(self_pet({kXingGuang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kIcebound),
                  "T2221.b 无异常对照：解除失败不落地");
        }
        std::cout << "  [T2221] 2158 解除门+对照 通过\n";
    }

    std::cout << "scenario_2220 ALL PASS" << std::endl;
    return 0;
}
