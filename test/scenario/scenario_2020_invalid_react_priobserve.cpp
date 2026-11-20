// 场景 2020 —— J108 技能无效反制/正先制观测族（2 宏，moves_lib/j_invalid_react_priobserve.cpp）
//（2026-10-03，worktree ../agents/j108；号段 2020-2029 → T2020-T2021 两块）
//
// 断言口径：
//   - 1794（彼空都·蜃气楼 [2538✗,1794]|[2,1,50] 属——2538 哑）：R1 挂窗（InRounds 2
//     → R1..R2）；R2 我方攻击 accuracy=1 → miss（EVENT_SKILL_INVALID）→ 回 max×1/2
//     （3000→5000）+ REDUCE 票；R3 对手大攻击（~480）→ -50%（掉血 ≈240 窄带）；
//     b) 窗过期（R4 miss）→ 无回血（血保持 3000）；
//   - 557（爆裂斩 [3,100,6] 物80 纯链——异常 6=害怕）：R1 挂窗（InRounds 3，tick 自
//     R2 起）；R2 对手电光火石（priority=1 正先制，10008）→ R2 ROUND_START tick
//     chance=100 → 害怕落地；对照 = 对手拍打（prio 0）→ 不触发。
//   - 557 载体攻击自身为物80 正常伤害（无扰）。
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

constexpr int kPaiDa  = 10004;   // 拍打：物40
constexpr int kDianHuo = 10008;  // 电光火石 priority=1：物40（正先制探针）
constexpr int kBaoLie  = 17521;  // 爆裂斩 [557]|[3,100,6]：物80（正先制观测载体）
constexpr int kPiKong  = 29398;  // 彼空都·蜃气楼 [2538,1794]|[2,1,50]：属（反制载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, 300, atk, kMaxHp);
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

    // ══ T2020 1794 技能无效 → 回 1/2 + 下次受击 -50%（彼空都·蜃气楼 [2,1,50]）══
    {
        // a) R1 挂窗；R2 我方 miss → 回满 + 票；R3 对手大攻击 → 半伤
        {
            ScenarioBattle b(
                make_party(mk_self({kPiKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1500)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;                  // 预置缺口（回 2500 → 满）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // R2：我方攻击 accuracy=1 且清 must_hit → 必 miss → 触发
            for (Skills& s : ctx.getPet(0).skills) {
                if (s.power > 0) {
                    s.accuracy = 1;
                    s.must_hit = 0;
                }
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T2020.a 1794：技能无效 → 回 max×1/2（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            // R3：对手大攻击 → REDUCE 票 -50%
            for (Skills& s : ctx.getPet(0).skills) {
                s.accuracy = 100;                     // 恢复命中（后续 miss 不再触发）
                s.must_hit = 1;
            }
            const int hp_before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int taken = hp_before - ctx.getPet(0).hp;
            CHECK(taken >= 200 && taken <= 280,
                  "T2020.a 1794：REDUCE 票半伤（实测掉 " + std::to_string(taken) + "）");
        }
        // b) 窗过期（[2] → R1..R2）：R4 miss 不再回血
        {
            ScenarioBattle b(
                make_party(mk_self({kPiKong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 空转
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 空转
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            // R4：我方攻击 miss（窗已过）→ 无回血
            for (Skills& s : ctx.getPet(0).skills) {
                s.accuracy = 1;
                s.must_hit = 0;
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 3000,
                     "T2020.b 1794：窗过期 miss 不回血（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T2020] 1794 技能无效反制 通过\n";
    }

    // ══ T2021 557 对手正先制 → 害怕（爆裂斩 [3,100,6]）══
    {
        // a) R1 挂窗（对手先用电光火石）；R2 对手再用电光火石（正先制）→ 害怕落地
        {
            ScenarioBattle b(
                make_party(mk_self({kBaoLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kDianHuo, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 我方拍打
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手电光火石
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, 6),
                  "T2021.a 557：对手正先制技 → 害怕(6) 落地");
        }
        // b) 对照：对手拍打（prio 0）→ 不触发
        {
            ScenarioBattle b(
                make_party(mk_self({kBaoLie, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手拍打
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, 6),
                  "T2021.b 557 对照：非正先制不触发");
        }
        std::cout << "  [T2021] 557 正先制观测 通过\n";
    }

    std::cout << "scenario_2020 PASS (J108 技能无效反制/正先制观测族 1794/557)" << std::endl;
    return 0;
}
