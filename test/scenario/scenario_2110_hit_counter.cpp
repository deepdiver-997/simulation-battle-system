// 场景 2110 —— J117 受击反击窗族（1 宏，moves_lib/j_hit_counter.cpp）
//（2026-10-03，worktree ../agents/j117；号段 2110-2119 → T2110 一块六例）
//
// 断言口径：
//   - 载体全属性技零伤害（对手承伤面唯一来源是 642 反击固伤，断言极净）；
//     档位表线性验证 2/3/4/5 = 150/200/250/300（码 0 仅 23513 70% 档未场景化，
//     码 1 未观测记档——出现即参数故障）；
//   - 攻击技门：对手属性技不产出红伤事件 → 不反击（T.b）；
//   - 窗 InRounds 含当回合（R1 武装、对手同回合命中即反击，T.a）+ 到期闲置
//     （{0} 回合外不反击，T.c R3 / T.f R3）；
//   - 完美控制 36609（族内唯一攻击技载体）拆两拍：R1 自攻对手无反击（无受击
//     事件）、R2 窗内受击反击恰 300——同链 1700 空场解弱失败/843 先制+2 均良性。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kBingBaba = 26576;  // 冰霸霸之护[48,47,642]|[3,3,3,100,2]：属性技（3回合·100%·强力150）
constexpr int kMingYe   = 27758;  // 冥夜深渊[4,4,642]|[0,100,1,4,100,2,2,100,2]：属性技（2回合·100%·强力150）
constexpr int kZhongKai = 27467;  // 重铠武装[48,47,642,843]|[4,4,4,100,4,2,2]：属性技（4回合·100%·绝对250）
constexpr int kGuLong   = 27835;  // 古龙残魂[4,4,4,4,57,642]|…[3,100,3]：属性技（3回合·100%·超强200）
constexpr int kWanMei   = 36609;  // 完美控制[1700,642,843]|[1,2,100,5,2,2]：特殊攻击150（2回合·100%·完美300）
constexpr int kPaiDa    = 10004;  // 拍打：物攻载体
constexpr int kGaoSu    = 20017;  // 高速移动：属性技（T.b 对手攻击技门对照）

constexpr int kMaxHp = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kGaoSu, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/200, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2110.a 冰霸霸之护 [3,100,2]：档位 2=150 + 窗含当回合 ════════════════
    {
        ScenarioBattle b(make_party(self_pet(kBingBaba)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装（先手 300>100）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打命中
        b.drive();
        const int taken = kMaxHp - ctx.getPet(0).hp;
        CHECK(taken >= 58 && taken <= 69,
              "T2110.a 前置：自身承伤带（实测 " + std::to_string(taken) + "）");
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(foe_loss, 150,
                 "T2110.a 642：同回合受击即反击恰 150（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2110.b 攻击技门：对手属性技命中无红伤 → 不反击 ═════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kBingBaba)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 对手高速移动（属性技）
        b.drive();
        const int taken = kMaxHp - ctx.getPet(0).hp;
        CHECK_EQ(taken, 0, "T2110.b 前置：属性技无伤害阶段（实测掉 " + std::to_string(taken) + "）");
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(foe_loss, 0,
                 "T2110.b 642：无红伤事件不反击（实测对手掉 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2110.c 冥夜深渊 [2,100,2]：窗跨 R1/R2、R3 到期闲置 ═════════════════
    {
        ScenarioBattle b(make_party(self_pet(kMingYe)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手拍打
        b.drive();
        int hp1 = ctx.getPet(1).hp;
        CHECK_EQ(kMaxHp - hp1, 150, "T2110.c 642：R1 反击恰 150");

        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // R2 自方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手拍打
        b.drive();
        const int r2 = hp1 - ctx.getPet(1).hp;
        CHECK_EQ(r2, 150, "T2110.c 642：R2 窗内仍反击恰 150（实测 " + std::to_string(r2) + "）");
        hp1 = ctx.getPet(1).hp;

        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // R3 自方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 对手拍打
        b.drive();
        const int r3 = hp1 - ctx.getPet(1).hp;
        CHECK_EQ(r3, 0, "T2110.c 642：R3 窗过期不反击（实测 " + std::to_string(r3) + "）");
    }
    // ══ T2110.d 重铠武装 [4,100,4]：档位 4=250 ══════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet(kZhongKai)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(foe_loss, 250,
                 "T2110.d 642：档位 4 反击恰 250（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2110.e 古龙残魂 [3,100,3]：档位 3=200（同链 57 回蓝/4 五连强化良性）═══
    {
        ScenarioBattle b(make_party(self_pet(kGuLong)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int foe_loss = kMaxHp - ctx.getPet(1).hp;
        CHECK_EQ(foe_loss, 200,
                 "T2110.e 642：档位 3 反击恰 200（实测 " + std::to_string(foe_loss) + "）");
    }
    // ══ T2110.f 完美控制 [2,100,5]：档位 5=300；R1 自攻无反击、R2 窗内反击 ═══
    {
        ScenarioBattle b(make_party(self_pet(kWanMei)), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自方特攻（atk=1 红伤 1~2）
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);          // R1 对手不出手
        b.drive();
        int hp1 = ctx.getPet(1).hp;
        const int r1 = kMaxHp - hp1;
        CHECK(r1 >= 1 && r1 <= 5,
              "T2110.f 前置：R1 仅自方红伤小带（实测 " + std::to_string(r1) + "）");

        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // R2 自方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 对手拍打命中
        b.drive();
        const int r2 = hp1 - ctx.getPet(1).hp;
        CHECK_EQ(r2, 300,
                 "T2110.f 642：R2 受击反击恰 300（实测 " + std::to_string(r2) + "）");
        hp1 = ctx.getPet(1).hp;

        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);          // R3 自方不出手
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 对手拍打
        b.drive();
        const int r3 = hp1 - ctx.getPet(1).hp;
        CHECK_EQ(r3, 0, "T2110.f 642：R3 两回合窗过期不反击（实测 " + std::to_string(r3) + "）");
    }

    std::cout << "scenario_2110 PASS (J117 受击反击窗族 642)" << std::endl;
    return 0;
}
