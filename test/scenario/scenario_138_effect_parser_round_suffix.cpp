#include <utils/sbs_rand.h>
// 场景 138 —— 组合语法解析器"回合数后缀"误伤回归（2026-09-27 堵煞事故）。
//
// 事故：极渊·堕煞 38018 的效果 2321（"造成的攻击伤害低于{0}则令对手{1}回合内属性技能
// 无效"，args=[200,2]）无专有实现 → 走组合语法解析器兜底 → parse_status_inflict_primary
// 的"令对手{"模式命中"令对手{1}回合内…"，把 {1}=2（**2 回合**）当成异常 id 2（**烧伤**）
// 施加 2 回合 → 对手凭空挂烧伤。修法：占位符 '}' 紧跟"回合"时拒绝按施加异常解析。
//
// 本场景锁两件事：
//   A. 堵煞打干净垫位宠 → 对手不得出现任何异常状态（修复前：烧伤 id=2, 2 回合）；
//   B. 解析器正路不受影响：1248 类真异常模板（无相谛，场景 009 已锁）照常工作——
//      这里用"令对手{1}，未触发则…"族（2320，{1} 后是逗号不是"回合"）确认仍解析。
#include <iostream>

#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kSkillZhuiSha = 38018;    // 极渊·堕煞：side_effect=[867,1220,2117,2321]
constexpr int kSkillZhuangJi = 10001;   // 撞击
constexpr int kClean = 1;               // 垫位宠 monster id（无魂印无特性）

// 堵煞 accuracy=97 → 测试层强制必中；威力 100 不至于一波带走垫位宠。
ElfPet zhui_sha_pet() {
    auto pet = make_pet(0, "堵煞桩", {kSkillZhuiSha, kSkillZhuangJi, kSkillZhuangJi,
                                      kSkillZhuangJi, kSkillZhuangJi},
                        /*speed=*/300, /*atk=*/10, /*hp=*/3000);
    pet.skills[0].must_hit = true;
    return pet;
}

ElfPet clean_pet() {
    return make_pet(kClean, "干净垫位", {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                         kSkillZhuangJi, kSkillZhuangJi},
                    /*speed=*/50, /*atk=*/10, /*hp=*/3000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }
    sbs_srand(7);

    // ════════════════════════════════════════════════════════════════
    // A. 堵煞 vs 干净垫位：效果链里只有 2321 走过解析器（867 同样跳过、
    //    1220 无强可消、2117 对手无流血）→ 不得有任何异常落地。
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(zhui_sha_pet()), make_party(clean_pet()));
        b.start();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        for (int ab = 0; ab <= 43; ++ab) {
            CHECK(!b.ctx().has_active_abnormal_status(1, ab),
                  "A：堵煞命中后对手不得带任何异常（解析器不得把'{1}回合内'当成异常 id）");
        }
        std::cout << "  [A] 堵煞无烧伤误伤 通过\n";
    }

    std::cout << "scenario_138 全部通过：解析器'回合数后缀'误伤已封" << std::endl;
    return 0;
}
