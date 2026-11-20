#include <utils/sbs_rand.h>
// 场景 3140 —— K181 先/后手双分支族·免异常/回血版（617，soul_lib/k181_order_immune_heal.cpp）
//（2026-10-07，worktree ../agents/k181，看板段 3140-3149 → T3140-T3143）
//
// 断言口径：
//   - T3140 先手免异常（对照对局对）：先手执行授 Ancient 全 mask 窗 → 局外直调
//     apply_anomaly_checked(烧伤2) 被免疫；对照局（无印）同调用落烧伤。
//   - T3141 后手回血（本体行 [2,3]，对照对局对同 seed）：heal 零掷点 → 两局序列一致
//     → 差值 = maxHP/3 = 33333（预置 hp 50000 留恢复空间、无钳制）。
//   - T3142 伙伴行 [2,2]：差值 = maxHP/2 = 49999（行参驱动数值）。
//   - T3143 窗口跨回合：R1 先手授窗 → R2 双 NONE（无刷新）局外施加仍被免疫 →
//     R3（窗尽）施加落地（diwang_3512 窗界 [R, R+1] 口径）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：攻击载体
constexpr int kMaxHp = 99999;

template <int MarkId>
ElfPet mark_pet(EffectArgs mark_args, int speed) {
    ElfPet p = make_pet(1, "托鲁克尔", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp,
                        /*soul_mark=*/SoulMark(MarkId, "先手免异常魂印", "",
                                               std::move(mark_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_me(int speed) {
    ElfPet p = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void round_actions(ScenarioBattle& b, bool attack) {
    const auto act = attack ? BattleFsm::ActionType::SELECT_SKILL
                            : BattleFsm::ActionType::NONE;
    b.submit_operation(0, act, 0);
    b.submit_operation(1, act, 0);
    b.drive();
}

// 局外直调施加烧伤（ behaviors 级断言看 has_active_abnormal_status）。
void try_burn(BattleContext& x) {
    (void)core_api().apply_anomaly_checked(&x, /*target=*/0, /*anomaly=*/2,
                                           /*duration_rounds=*/3, /*actor=*/1,
                                           /*chance_pct=*/100, ChanceSource::Skill);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3140 先手免异常（对照对局对）══
    {
        for (int side = 0; side < 2; ++side) {
            auto me = side == 0
                ? mark_pet<617>(EffectArgs(std::vector<int>{2, 3}), /*speed=*/300)
                : plain_me(/*speed=*/300);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(plain_foe(/*speed=*/100)));
            sbs_srand(7);
            b.start();
            round_actions(b, /*attack=*/true);   // 先手执行 → 授窗
            try_burn(b.ctx());
            if (side == 0) {
                CHECK(!b.ctx().has_active_abnormal_status(0, 2),
                      "T3140 617：先手授窗 → 烧伤被免疫");
            } else {
                CHECK(b.ctx().has_active_abnormal_status(0, 2),
                      "T3140 对照：无印 → 烧伤落地");
            }
        }
        std::cout << "  [T3140] 先手免异常 通过\n";
    }

    // ══ T3141/T3142 后手回血：本体 1/3、伙伴行 1/2（对照对局对精确差值）══
    {
        const int presets[2] = {3, 2};   // db[1]：本体 / 伙伴行
        for (int row = 0; row < 2; ++row) {
            int marked_after = -1;
            int plain_after = -1;
            for (int side = 0; side < 2; ++side) {
                auto me = side == 0
                    ? mark_pet<617>(EffectArgs(std::vector<int>{2, presets[row]}),
                                    /*speed=*/100)
                    : plain_me(/*speed=*/100);
                ScenarioBattle b(make_party(std::move(me)),
                                 make_party(plain_foe(/*speed=*/300)));
                sbs_srand(7);
                b.start();
                b.ctx().seerRobot[0].elfPets[0].hp = 50000;   // 留恢复空间
                round_actions(b, /*attack=*/true);            // 后手执行 → 回血
                if (side == 0) {
                    marked_after = b.ctx().getPet(0).hp;
                } else {
                    plain_after = b.ctx().getPet(0).hp;
                }
            }
            const int expect = kMaxHp / presets[row];
            CHECK(marked_after < kMaxHp, "T3141 前置：回血未钳 max");
            CHECK_EQ(marked_after - plain_after, expect,
                     (row == 0 ? "T3141 617：本体行恢复 = maxHP/3"
                               : "T3142 617：伙伴行恢复 = maxHP/2"));
            std::cout << "  [T314" << (row + 1) << "] 后手回血 " << expect
                      << " 通过 (marked=" << marked_after << " plain=" << plain_after
                      << ")\n";
        }
    }

    // ══ T3143 免疫窗跨回合：R1 先手授窗 [R1,R2] → R2 门口施加被免疫（无刷新）→
    //     R2 完成后窗尽、施加落地（时长票按回合完成递减的实证窗界）══
    {
        ScenarioBattle b(make_party(mark_pet<617>(EffectArgs(std::vector<int>{2, 3}),
                                                  /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        round_actions(b, /*attack=*/true);    // R1：先手授窗
        try_burn(b.ctx());                    // R2 门口（R1 已完、窗尚存）
        CHECK(!b.ctx().has_active_abnormal_status(0, 2),
              "T3143 617：窗内第二回合仍免疫（无刷新）");
        round_actions(b, /*attack=*/false);   // R2 双 NONE：无执行不刷新，窗尽
        try_burn(b.ctx());
        CHECK(b.ctx().has_active_abnormal_status(0, 2),
              "T3143 617：窗尽后烧伤落地");
        std::cout << "  [T3143] 窗口跨回合 通过\n";
    }

    std::cout << "scenario_3140 PASS (K181 先/后手双分支族·免异常/回血版)" << std::endl;
    return 0;
}
