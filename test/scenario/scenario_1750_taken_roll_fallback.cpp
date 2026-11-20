// 场景 1750 —— J85 受击掷骰族（1 宏，moves_lib/j_taken_roll_fallback.cpp）
//（2026-10-03，worktree ../agents/j85；号段 1750-1759 → T1750 两块）
//
// 断言口径：
//   - 载体极速幻影 25545 [1194,1195,1017]|[4,4,100,19,300]：属性技零伤害（自身不产出
//     红伤——对手承伤面唯一来源是 1195 的 300 固伤，断言极净）；100% 档命中路径确定；
//     1194=反转自身下降+免降窗（空场反转失败=良性，已注册 canglan）；
//   - 未触发路径 = 对手 R1 用凝神聚力斩 17091 [48]|[4]（4 回合免疫异常）→ 自身 R1
//     挨拍打（atk=200 带 [58,69]）→ apply 必 TARGET_IMMUNE → 兜底 300 固伤（恰 300，
//     无红伤混杂）；
//   - 瘫痪(19) 无扣血 tick（中毒会污染 hp 差分，J16 同款选型）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打
constexpr int kJiSu   = 25545;   // 极速幻影[1194,1195,1017]|[4,4,100,19,300]：属性技（4回合窗·100%瘫痪·兜底300固伤）
constexpr int kNingShen = 17091; // 凝神聚力斩[48]|[4]：物150（对手用：4回合免疫异常）

constexpr int kCrippled = static_cast<int>(AbnormalStatusId::Crippled); // 瘫痪=19（Crippled；勿用 Paralysis=0 麻痹）
constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 200) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1750 1195 受击掷骰·未触发固伤兜底（极速幻影 [4,100,19,300]）══
    {
        // a) 命中路径：R1 自身武装；对手拍打 → 自身掉 [58,69] 且对手 100% 瘫痪（无固伤）
        {
            ScenarioBattle b(make_party(self_pet({kJiSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打
            b.drive();
            const int taken = kMaxHp - ctx.getPet(0).hp;
            CHECK(taken >= 58 && taken <= 69,
                  "T1750.a 前置：自身承伤带（实测 " + std::to_string(taken) + "）");
            CHECK(ctx.has_active_abnormal_status(1, kCrippled),
                  "T1750.a 1195：受击 100% 瘫痪落地");
            CHECK_EQ(kMaxHp - ctx.getPet(1).hp, 0,
                     "T1750.a 1195：命中路径无兜底固伤（对手满血，实测掉 " +
                         std::to_string(kMaxHp - ctx.getPet(1).hp) + "）");
        }
        // b) 兜底路径：对手免疫 → apply 必败 → 对手恰吃 300 固伤
        {
            ScenarioBattle b(make_party(self_pet({kJiSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kNingShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手开免疫
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kCrippled) == false,
                  "T1750.b 前置：免疫期瘫痪未落地");
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK_EQ(foe_loss, 300,
                     "T1750.b 1195：未触发兜底恰 300 固伤（实测 " +
                         std::to_string(foe_loss) + "）");
        }
        std::cout << "  [T1750] 1195 受击掷骰 通过\n";
    }

    std::cout << "scenario_1750 PASS (J85 受击掷骰族 1195)" << std::endl;
    return 0;
}
