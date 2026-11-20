// 场景 5610 —— J230 受击转化护盾族（1768，moves_lib/j_taken_to_shield.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5610-5619）
//
// 断言口径（护盾总量 + hp 掉血 + 分型过滤 + 计次）：
//   - 1768（圣洁岩盾 27848 [191,738,1768]|[4,3,2,1,300]）：{0}=1 次、{1}=300 上限。
//   - a) atk600/拍打 D∈[174,204]：护盾总量 == 本刀掉血 D，且落在实测带。
//   - b) atk1200/拍打 D∈[348,408]：转化封顶 300（掉血仍为 D）。
//   - c) 粉伤 500 不转化（只认 NORMAL 红伤）：护盾 0。
//   - d) 计次 1 用尽：R2 高威力普攻（神风烈踢 150）吃光首盾，不再补盾（护盾 0）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;  // 拍打：物 40
constexpr int kShenFeng = 10181; // 神风烈踢：物 150（无附带效果）
constexpr int kShengJie = 27848; // [191,738,1768]|[4,3,2,1,300]
constexpr int kMaxHp   = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kShengJie, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kShenFeng, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/atk, /*hp=*/kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

void self_cast_foe_attacks(ScenarioBattle& b, int foe_skill_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_skill_slot);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5610 1768 受击伤害转化护盾（counts=1, cap=300）══
    {
        {   // (a) 普通攻击：护盾 == 掉血 D
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/600)));
            b.start();
            BattleContext& ctx = b.ctx();
            self_cast_foe_attacks(b, /*foe_skill_slot=*/0);
            const int loss = kMaxHp - ctx.getPet(0).hp;
            const int shield = ctx.getPet(0).shield_bank_.total();
            CHECK(loss >= 174 && loss <= 204,
                  "T5610.a 1768：atk600 拍打掉血带 [174,204]（实测 " +
                      std::to_string(loss) + "）");
            CHECK_EQ(shield, loss,
                     "T5610.a 1768：护盾总量 == 本刀攻击伤害（护盾 " +
                         std::to_string(shield) + " / 掉血 " + std::to_string(loss) + "）");
        }
        {   // (b) 高攻：转化封顶 300
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/1200)));
            b.start();
            BattleContext& ctx = b.ctx();
            self_cast_foe_attacks(b, /*foe_skill_slot=*/0);
            const int loss = kMaxHp - ctx.getPet(0).hp;
            CHECK(loss >= 340 && loss <= 415,
                  "T5610.b 1768：atk1200 掉血带 [340,415]（实测 " +
                      std::to_string(loss) + "）");
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 300,
                     "T5610.b 1768：转化最多获得 300（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        {   // (c) 粉伤不转化（对手挂机 → 计次未被红伤消耗）
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/1)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T5610.c 前置：R1 对手挂机 → 无红伤转化（护盾 0）");
            const int before = ctx.getPet(0).hp;
            (void)deal_pink_damage(&ctx, 0, 500, DamageKind::FIXED, 1);
            // 事件队列在回合推进时 drain；下回合双方挂机。
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T5610.c 1768：粉伤不转化（护盾仍 0）");
            CHECK_EQ(before - ctx.getPet(0).hp, 500,
                     "T5610.c 1768：粉伤 500 正常落地");
        }
        {   // (d) 计次 1 用尽：R2 高威力普攻吃光首盾，不再补盾
            ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(/*atk=*/600)));
            b.start();
            BattleContext& ctx = b.ctx();
            self_cast_foe_attacks(b, /*foe_skill_slot=*/0);
            const int first_shield = ctx.getPet(0).shield_bank_.total();
            CHECK(first_shield >= 174 && first_shield <= 204,
                  "T5610.d 前置：首盾带 [174,204]（实测 " +
                      std::to_string(first_shield) + "）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);  // 神风烈踢 150
            b.drive();
            CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                     "T5610.d 1768：计次用尽 → 高威力普攻吃光护盾后不再补盾（实测 " +
                         std::to_string(ctx.getPet(0).shield_bank_.total()) + "）");
        }
        std::cout << "  [T5610] 1768 伤害转化 / 封顶 / 粉伤过滤 / 计次用尽 通过\n";
    }

    std::cout << "scenario_5610 PASS (J230 受击转化护盾族)" << std::endl;
    return 0;
}
