// 场景 1720 —— J82 受击转化/先手免反族（2 宏，moves_lib/j_taken_convert_reflect.cpp）
//（2026-10-03，worktree ../agents/j82；号段 1720-1729 → T1720-T1721 两块）
//
// 断言口径：
//   - 1314 = 受击后等额 heal（净伤 0）：幽夜漂浮 [1]——R1 用技 → 对手拍打（atk=200
//     带 [58,69]）落地后全额回 → 自身 hp 与受击前持平；第二击（窗已用完）真掉带；
//   - 138 = 先手 BLOCK 免伤+反弹（光之盾 [1,2]：1 回合·反弹 1/2）——先手出技 →
//     对手同回合拍打被归零（自身 0 掉血）且对手吃免疫量一半（[29,34]）；后手门负例
//     = 慢速自身出技不武装（对手先手那下照打）；窗过期对照 = 第二回合承伤回带；
//   - 载体双属性技零伤害零随机；链全纯。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物40（探针攻击）
constexpr int kYouYe  = 28588;   // 幽夜漂浮[1314]|[1]：属性技（下1次受击转化）
constexpr int kGuangZhi = 21328; // 光之盾[138]|[1,2]：属性技（先手1回合免伤·反弹1/2）

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

    // ══ T1720 1314 下 1 次受击转化为体力（幽夜漂浮 [1]）══
    {
        // R1 用技武装；R1 对手拍打（第 1 击：转化=净 0）；R2 对手拍打（第 2 击：真掉）
        ScenarioBattle b(make_party(self_pet({kYouYe, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打
        b.drive();
        const int hp_after_r1 = ctx.getPet(0).hp;
        CHECK_EQ(hp_after_r1, kMaxHp,
                 "T1720 1314：第 1 击转化为体力（净伤 0，实测损 " +
                     std::to_string(kMaxHp - hp_after_r1) + "）");
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手拍打
        b.drive();
        const int taken_r2 = kMaxHp - ctx.getPet(0).hp - (kMaxHp - hp_after_r1) * 0;
        const int loss_r2 = hp_after_r1 - ctx.getPet(0).hp;
        CHECK(loss_r2 >= 58 && loss_r2 <= 69,
              "T1720 1314：第 2 击预算耗尽真掉（实测 " + std::to_string(loss_r2) +
                  "，带 [58,69]）");
        (void)taken_r2;
        std::cout << "  [T1720] 1314 受击转化 通过\n";
    }

    // ══ T1721 138 先手免伤+反弹 1/2（光之盾 [1,2]）══
    {
        // a) 正例：先手出技 → 对手同回合拍打被归零 + 对手吃反弹半额；窗 1 回合
        {
            ScenarioBattle b(make_party(self_pet({kGuangZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 300)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 先手出技
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1721.a 138：自身免伤（实测损 " +
                         std::to_string(kMaxHp - ctx.getPet(0).hp) + "）");
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            CHECK(foe_loss >= 29 && foe_loss <= 34,
                  "T1721.a 138：反弹 1/2 免疫量（实测 " + std::to_string(foe_loss) +
                      "，带 [29,34]）");
            const int hp0 = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手拍打
            b.drive();
            const int taken_r2 = hp0 - ctx.getPet(0).hp;
            CHECK(taken_r2 >= 58 && taken_r2 <= 69,
                  "T1721.a 138：窗过期 R2 承伤回带（实测 " + std::to_string(taken_r2) + "）");
        }
        // b) 后手门负例：慢速自身 → 不武装 → 对手先手那下照打、无反弹
        {
            ScenarioBattle b(make_party(self_pet({kGuangZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 100)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, 200)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int taken = kMaxHp - ctx.getPet(0).hp;
            CHECK(taken >= 58 && taken <= 69,
                  "T1721.b 138 门：后手不武装（实测承伤 " + std::to_string(taken) + "）");
            CHECK_EQ(ctx.getPet(1).hp, kMaxHp, "T1721.b 138 门：无反弹（对手满血）");
        }
        std::cout << "  [T1721] 138 先手免反 通过\n";
    }

    std::cout << "scenario_1720 PASS (J82 受击转化/先手免反族 1314/138)" << std::endl;
    return 0;
}
