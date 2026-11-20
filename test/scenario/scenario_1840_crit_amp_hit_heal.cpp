// 场景 1840 —— J95 暴击增幅/受伤回满反噬族（2 宏，moves_lib/j_crit_amp_hit_heal.cpp）
//（2026-10-03，worktree ../agents/j95；号段 1840-1849 → T1840-T1841 两块）
//
// 断言口径：
//   - 1840（镇土铁碎阵 [50] 物150 短链）：必暴档（crit 旗 → amp +50%，且暴击本身
//     ×2.3 实测倍率）对零暴对照 → 伤害比 ≈3.46（带 [2.9, 3.5]，双方各吃浮动）；
//   - 1073（惊天战意 [4,4,3,300] 属 短链——191/47 兄弟均实现良性）：预置缺口 3000
//     → 对手一击 480（>300 阈值）→ 触发：回满 5000 整 + 等量反噬 = 恢复量 2480
//     （fixed 无浮动、我方载体属性技无红伤 → 对手掉血精确 2480）；
//     对照 = 对手小攻击（~2 <300）→ 不触发。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kZhenTu = 37043;  // 镇土铁碎阵 [1840,1323]|[50,50]：物150（暴击增幅载体）
constexpr int kJingTian = 25842; // 惊天战意 [191,47,1073]|[4,4,3,300]：属（受伤回满载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/100, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, 100, atk, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1840 1840 暴击 → 伤害 +50%（镇土铁碎阵 [50]）══
    {
        // a) 必暴：暴击 ×2 × amp 1.5 ≈ 3× 基础
        int d_crit = 0;
        {
            ScenarioBattle b(
                make_party(mk_self({kZhenTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            set_crit_sixteenths(ctx.getPet(0), 16);
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_crit = 99999 - ctx.getPet(1).hp;
        }
        // b) 零暴对照：无 amp、无暴击倍率
        int d_plain = 0;
        {
            ScenarioBattle b(
                make_party(mk_self({kZhenTu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            d_plain = 99999 - b.ctx().getPet(1).hp;
        }
        CHECK(d_plain > 0, "T1840 前置：基础伤 > 0（实测 " + std::to_string(d_plain) + "）");
        CHECK(d_crit * 10 >= d_plain * 29 && d_crit * 10 <= d_plain * 35,
              "T1840.a 1840：暴击+amp≈3.5×（暴 " + std::to_string(d_crit) +
                  " / 平 " + std::to_string(d_plain) + "）");
        std::cout << "  [T1840] 1840 暴击增幅 通过\n";
    }

    // ══ T1841 1073 受击>300 → 回满+等量反噬（惊天战意 [3,300]）══
    {
        // a) 对手一击 480（>300）→ 回满 5000 + 反噬 = 恢复量 2480（精确）
        {
            ScenarioBattle b(
                make_party(mk_self({kJingTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1500)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;                  // 预置缺口
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // R2：对手攻击 ~480（>300）→ 触发
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1841.a 1073：受击 >300 → 回满（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            CHECK(foe_loss >= 2480 && foe_loss <= 2510,
                  "T1841.a 1073：反噬=恢复量 2480（实测掉 " +
                      std::to_string(foe_loss) + "）");
        }
        // b) 对照：小攻击（~2 <300）→ 不触发
        {
            ScenarioBattle b(
                make_party(mk_self({kJingTian, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 我方拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手小攻击
            b.drive();
            const int self_hp = ctx.getPet(0).hp;
            CHECK(self_hp < 3000 && self_hp >= 2990,
                  "T1841.b 1073 对照：小伤不触发（我方血 " +
                      std::to_string(self_hp) + "）");
            CHECK(99999 - ctx.getPet(1).hp <= 60,
                  "T1841.b 1073 对照：无反噬（对手仅吃我方拍打，实测掉 " +
                      std::to_string(99999 - ctx.getPet(1).hp) + "）");
        }
        std::cout << "  [T1841] 1073 受击回满反噬 通过\n";
    }

    std::cout << "scenario_1840 PASS (J95 暴击增幅/受伤回满反噬族 1840/1073)" << std::endl;
    return 0;
}
