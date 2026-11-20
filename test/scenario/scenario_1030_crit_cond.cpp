// 场景 1030 —— J18 暴击条件族（2 宏，moves_lib/j_crit_cond.cpp）
//（2026-10-02，worktree ../agents/j18；号段 1030-1039 → T1030-T1031 两块）
//
// 断言口径：
//   - 1204 用 crit_happened 旗断言（必暴 → 旗置位；旗在每次技能使用先清，攻击技能
//     才掷暴击——make_pet 默认 crit_rate=0，"旗为 false"= 真没暴，不靠伤害倍率猜）；
//   - 1483 用封属的**行为面**断言：对手（后手）属性技=神经修复（回 1/3 最大体力），
//     被封 = 不回血，恢复 = 精确回 33333；必暴侧 set_crit_sixteenths(16)，
//     负例默认 0 率；
//   - 载体链排查：28331 灼烈之风 [1204] 单效果（属性技——自身无红伤、当次不掷暴）；
//     33760 幽魂铁蹄 [1483] 单效果。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;   // 拍打：物40（探针攻击，crit_rate=0）
constexpr int kFengYun = 28331;  // 灼烈之风[1204]|[2]：属（下2次攻击必暴）
constexpr int kYouHun  = 33760;  // 幽魂铁蹄[1483]|[1]：物150（暴击→封对手属性1回合）
constexpr int kXiuFu   = 20309;  // 神经修复[43]：属（回 1/3，对手探针用）
constexpr int kMaxHp   = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = kMaxHp) {
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

    // ══ T1030 1204 下2次攻击必暴（灼烈之风 [1204]|[2]，次数窗）══
    {
        ScenarioBattle b(make_party(self_pet({kFengYun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体（属）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.crit_happened[0], "T1030.a 1204：载体当次不算（下2次的'下'）");
        for (int r = 2; r <= 3; ++r) {   // R2/R3：两次必暴拍打
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.crit_happened[0],
                  "T1030.b 1204：第 " + std::to_string(r - 1) + " 次攻击必暴（crit 旗置位）");
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4：次数耗尽
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(!ctx.crit_happened[0], "T1030.c 1204：2 次用尽不再必暴（默认 0 率）");
        std::cout << "  [T1030] 1204 次数必暴窗 通过\n";
    }

    // ══ T1031 1483 本回合暴击 → 封对手属性技 1 回合（幽魂铁蹄 [1483]|[1]）══
    {
        // a) 必暴：R1 暴击 → 对手 R1 属性技无效（不回血）、R2 恢复（回 33333）
        {
            ElfPet me = self_pet({kYouHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            set_crit_sixteenths(me, 16);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = kMaxHp / 2;   // 压当前血（base 不动，修复 1/3 可观测）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 暴击
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //     后手修复
            b.drive();
            CHECK(ctx.crit_happened[0], "T1031.a 前置：R1 打出暴击");
            CHECK(ctx.getPet(1).hp < kMaxHp / 2 + kMaxHp / 3,
                  "T1031.a 1483：暴击当回合对手属性技无效（未回血，hp=" +
                      std::to_string(ctx.getPet(1).hp) + "）");
            const int hp_r1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 再修复
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp - hp_r1, kMaxHp / 3,
                     "T1031.a 1483：封属 1 回合后恢复（实测回 " +
                         std::to_string(ctx.getPet(1).hp - hp_r1) + "）");
        }
        // b) 负例：没暴击（默认 0 率）→ 不封，对手 R1 修复照常
        {
            ScenarioBattle b(make_party(self_pet({kYouHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kXiuFu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(1).hp = kMaxHp / 2;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK(!ctx.crit_happened[0], "T1031.b 前置：未暴击");
            CHECK(ctx.getPet(1).hp > kMaxHp / 2,
                  "T1031.b 1483 门：未暴击不封属（对手修复照常，hp=" +
                      std::to_string(ctx.getPet(1).hp) + "）");
        }
        std::cout << "  [T1031] 1483 暴击封属 通过\n";
    }

    std::cout << "scenario_1030 PASS (J18 暴击条件族 1204/1483)" << std::endl;
    return 0;
}
