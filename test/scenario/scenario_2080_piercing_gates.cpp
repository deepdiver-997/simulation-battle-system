// 场景 2080 —— J114 先出手门族二·穿透门（750/1508，
// moves_lib/j_first_move_gates2.cpp；2026-10-03，worktree ../agents/j114）
//
// 断言口径：
//   - 750 先手穿 CAP：foe 用移山填海 17357 [766]|[3,200]（消强成功→3回合我方攻击≤200）
//     —— R1 我强化、R2 foe 消强+锁帽（pri=2 先手）、R3 我攻击：
//       (a) 对照·拍打 → 恰 200（CAP 钳制）；
//       (b) 750·激光刀（pri=3 先手）→ 穿帽 loss==red>200；
//       (c) 750·混沌利爪（pri=0、我速 50<400 后手）→ 恰 200（先手门不成立）。
//   - 1508 先手穿盔：R1 foe 用风萧影抖 29005（1293 免疫下1次对手攻击·可穿·被穿消耗）：
//       (a) 对照·拍打 → 被盔无效 loss==0；
//       (b) 1508·超电弧（先手）→ 穿盔 loss==red>0；
//       (c) 1508·超电弧（后手）→ 被盔无效 loss==0。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kGaoSu   = 20017;   // 高速移动 [4]|[4,100,2]（R1 强化，供 766 消强成功）
constexpr int kPaiDa   = 10004;   // 拍打：物40（对照攻击刀）
constexpr int kJiGuang = 19011;   // 激光刀 [750] 物80 pri=3（先手 750 载体）
constexpr int kHunDun  = 17201;   // 混沌利爪 [750] 物150 pri=0（后手 750 载体）
constexpr int kYiShan  = 17357;   // 移山填海 [766]|[3,200]（foe CAP 生产者）
constexpr int kDianHu  = 37733;   // 超电弧 [1508] 特140（1508 载体）
constexpr int kFengXiao = 29005;  // 风萧影抖 [191,1293,1647,1655]（1293 可穿盔武装）

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1000, int hp = 5000) {
    ElfPet p = make_pet(1, "穿门族", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int hp = 99999) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

// 三回合舞步：R1 我强化/foe 过 → R2 我过/foe 消强+锁帽 → R3 我攻击/foe 过。
// 返回前先跑完 R1-R2，调用方提交 R3 操作后 drive。
void arm_cap_then(ScenarioBattle& b, int self_attack_index_or_none) {
    // R1
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 高速移动（强化）
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    // R2
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 移山填海（消强+锁帽）
    b.drive();
    // R3（由调用方提交）
    (void)self_attack_index_or_none;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2080 750 先出手无视伤害限制（766 CAP 消费端集成）══
    {
        {   // (a) 对照：无 750，R3 拍打 → CAP 钳到恰 200
            ScenarioBattle b(make_party(self_pet({kGaoSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kYiShan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            arm_cap_then(b, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK_EQ(loss, 200, "T2080.a 750 对照：CAP 钳制恰 200（实测 " +
                                    std::to_string(loss) + "）");
        }
        {   // (b) 750 先手（激光刀 pri=3）：穿帽，loss==red>200
            ScenarioBattle b(make_party(self_pet({kGaoSu, kJiGuang, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kYiShan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            arm_cap_then(b, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK(red > 200, "T2080.b 前置：激光刀红伤过帽（red=" + std::to_string(red) + "）");
            CHECK_EQ(loss, red, "T2080.b 750 先手穿帽：loss==red（loss=" +
                                    std::to_string(loss) + "）");
        }
        {   // (c) 750 后手（混沌利爪 pri=0、速 50<400）：先手门不成立 → 恰 200
            ScenarioBattle b(make_party(self_pet({kGaoSu, kHunDun, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/50)),
                             make_party(foe_pet({kYiShan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            arm_cap_then(b, 1);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK_EQ(loss, 200, "T2080.c 750 后手不穿：CAP 恰 200（实测 " +
                                    std::to_string(loss) + "）");
        }
        std::cout << "  [T2080] 750 先手穿伤害限制+对照 通过\n";
    }

    // ══ T2081 1508 先出手无视攻击免疫（1293 可穿盔消费端集成）══
    {
        {   // (a) 对照：R1 foe 武装盔，R2 我拍打 → 被无效
            ScenarioBattle b(make_party(self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kFengXiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 武装盔
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK_EQ(loss, 0, "T2081.a 1508 对照：攻击被盔无效（实测 " +
                                  std::to_string(loss) + "）");
        }
        {   // (b) 1508 先手（超电弧，速 300>100）：穿盔落地
            ScenarioBattle b(make_party(self_pet({kDianHu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kFengXiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 超电弧
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            const int red = ctx.resolvedDamage.final;
            CHECK(red > 0, "T2081.b 前置：超电弧有红伤（red=" + std::to_string(red) + "）");
            CHECK_EQ(loss, red, "T2081.b 1508 先手穿盔：loss==red（loss=" +
                                    std::to_string(loss) + "）");
        }
        {   // (c) 1508 后手（速 50<400）：先手门不成立 → 被无效
            ScenarioBattle b(make_party(self_pet({kDianHu, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/50)),
                             make_party(foe_pet({kFengXiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = 99999 - ctx.getPet(1).hp;
            CHECK_EQ(loss, 0, "T2081.c 1508 后手不穿：被盔无效（实测 " +
                                  std::to_string(loss) + "）");
        }
        std::cout << "  [T2081] 1508 先手穿攻击免疫+对照 通过\n";
    }

    std::cout << "scenario_2080 ALL PASS" << std::endl;
    return 0;
}
