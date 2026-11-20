// 场景 1910 —— J100 消回合双分支/闪避窗族（2 宏，moves_lib/j_break_noeffect_dodge.cpp）
//（2026-10-03，worktree ../agents/j100；号段 1910-1919 → T1910-T1911 两块）
//
// 断言口径：
//   - 1777（浅滩浮游 [100,28] 特85——447 兄弟=伤害下限，不扰异常断言）双分支：
//     a) 对手预置回合类效果（灼烈之风 28331 [1204]|[2]，R1 挂 NextRounds 窗）→
//        R2 消除 SUCCESS → 无后续（对手不带异常 28）；
//     b) 对手无回合类效果 → NO_EFFECTS → {0}%=100 令对手凝滞(28)（确定性）；
//   - 991（瀚空之诺 [1,100] 特130 纯链）：R1 载体攻击（atk=1 对手存活 → 门开）→
//     R2 对手攻击：EVENT_SKILL_USED（query_usage 之前）抬 dodge 100% → 必闪
//     （我方血 5000 整）；对照 = 不挂载体 → 对手命中（我方掉血）。
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

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kQianTan = 36239; // 浅滩浮游 [1777,447]|[100,28,200]：特85（消回合双分支载体）
constexpr int kHanKong = 36117; // 瀚空之诺 [991]|[1,100]：特130（闪避窗载体）
constexpr int kZhuoLie = 28331; // 灼烈之风 [1204]|[2]（对手预置回合类效果）
constexpr int kMaxHp  = 5000;
constexpr int kStasis = 28;     // 凝滞

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1910 1777 消回合双分支（浅滩浮游 [100,28]）══
    {
        // a) 对手有回合类效果 → SUCCESS 分支 → 无后续
        {
            ScenarioBattle b(
                make_party(mk_self({kQianTan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kZhuoLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            // R1：对手挂窗（1204 NextRounds 窗=回合类效果）；我方拍打过渡
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            // R2：消对手回合类 → SUCCESS
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kStasis),
                  "T1910.a 1777：SUCCESS 分支无后续（对手无凝滞）");
        }
        // b) 对照：对手无回合类效果 → NO_EFFECTS → 凝滞(28) 100% 落地
        {
            ScenarioBattle b(
                make_party(mk_self({kQianTan, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 直接消
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kStasis),
                  "T1910.b 1777 对照：NO_EFFECTS 分支 → 凝滞落地");
        }
        std::cout << "  [T1910] 1777 消回合双分支 通过\n";
    }

    // ══ T1911 991 未击败 → 对手下 1 次攻击 100% 被闪避（瀚空之诺 [1,100]）══
    {
        // a) R1 载体（atk=1 对手存活 → 门开）；R2 对手攻击 → 必闪
        {
            ScenarioBattle b(
                make_party(mk_self({kHanKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 我方拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手攻击
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1911.a 991：下 1 次攻击 100% 被闪避（我方血 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 对照：未挂载体 → 对手命中
        {
            ScenarioBattle b(
                make_party(mk_self({kHanKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 拍打（无载体）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 我方拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手攻击
            b.drive();
            CHECK(ctx.getPet(0).hp < kMaxHp,
                  "T1911.b 991 对照：无闪避窗 → 命中（我方血 " +
                      std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1911] 991 闪避窗 通过\n";
    }

    std::cout << "scenario_1910 PASS (J100 消回合双分支/闪避窗族 1777/991)" << std::endl;
    return 0;
}
