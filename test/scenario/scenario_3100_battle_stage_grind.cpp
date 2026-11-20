#include <utils/sbs_rand.h>
// 场景 3100 —— K178 攻击技战阶磨血族（351，soul_lib/k178_battle_stage_grind.cpp）
//（2026-10-07，worktree ../agents/k178，看板段 3100-3109 → T3100-T3103）
//
// 断言口径：
//   - T3100 扎尔瑟隆行 [1000,5]（100% 档，对照对局对，同 seed 同操作）：roll_percent(100)
//     零掷点 → 两局 rand 序列一致 → 对手失血 = 攻击伤害（对照同值）+ maxHP/5（99999/5
//     = 19999）精确相等。
//   - T3101 张辽行 [500,5]（50% 档，统计断言 K12 2920 口径）：20 回合每回合攻击→掷点，
//     回合末 foe 单回失血 ≥19999 判一次磨血（攻击红伤仅个位-十位，阈值 10000 干净分离）；
//     Binomial(20,0.5) σ≈2.24 → 断言 4 ≤ 磨血次数 ≤ 16（≈±2.7σ）。
//   - T3102 张辽行 boss 门（monster_id 2770 × is_boss_challenge）：20 回合 0 次磨血
//     （确定性强断言——闸门失效则 50% 档必有落地）。
//   - T3103 计数旗消费（扎尔瑟隆行）：R1 双攻 → R2 双 NONE → R2 对手失血 = 0
//     （旗已清、无使用无磨血——防旗残留双跳）。
#include <array>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;       // 拍打：攻击载体
constexpr int kMaxHp = 99999;
constexpr int kGrind = kMaxHp / 5; // 19999

template <int MarkId>
ElfPet mark_pet(int monster_id, EffectArgs mark_args, int speed) {
    // ⚠️ 行级 boss 门读 soulMark.monster_id（DB 行才有，场景构造须显式补——2890 同款）。
    SoulMark sm(MarkId, "战阶磨血魂印", "", std::move(mark_args));
    sm.monster_id = monster_id;
    ElfPet p = make_pet(monster_id, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/kMaxHp, /*soul_mark=*/std::move(sm));
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3100 100% 档：磨血 = maxHP/5（对照对局对精确断言）══
    {
        int marked_foe_loss = -1;
        int plain_foe_loss = -1;
        for (int side = 0; side < 2; ++side) {
            auto me = side == 0
                ? mark_pet<351>(/*扎尔瑟隆*/ 2729,
                                EffectArgs(std::vector<int>{1000, 5}), /*speed=*/300)
                : plain_me(/*speed=*/300);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(plain_foe(/*speed=*/100)));
            sbs_srand(7);
            b.start();
            round_actions(b, /*attack=*/true);
            const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
            if (side == 0) {
                marked_foe_loss = foe_loss;
            } else {
                plain_foe_loss = foe_loss;
            }
        }
        CHECK(marked_foe_loss > kGrind, "T3100 前置：磨血落地");
        CHECK_EQ(marked_foe_loss, plain_foe_loss + kGrind,
                 "T3100 351：对手失血 = 攻击伤害（同 seed 同值）+ maxHP/5");
        std::cout << "  [T3100] 100% 磨血 通过 (marked=" << marked_foe_loss
                  << " plain=" << plain_foe_loss << " grind=" << kGrind << ")\n";
    }

    // ══ T3101 50% 档：20 回合统计带 ══
    {
        ScenarioBattle b(make_party(mark_pet<351>(/*张辽*/ 2770,
                                                  EffectArgs(std::vector<int>{500, 5}),
                                                  /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(11);
        b.start();
        int grinds = 0;
        for (int r = 0; r < 20; ++r) {
            const int before = b.ctx().getPet(1).hp;
            round_actions(b, /*attack=*/true);
            const int loss = before - b.ctx().getPet(1).hp;
            if (loss >= 10000) {
                ++grinds;   // 攻击红伤个位-十位，≥10000 必含磨血
            }
        }
        CHECK(grinds > 0, "T3101 前置：50% 档有落地");
        CHECK(grinds >= 4 && grinds <= 16,
              "T3101 351：20 回合磨血次数落 Binomial(20,0.5) 统计带 [4,16]");
        std::cout << "  [T3101] 50% 统计带 通过 (grinds=" << grinds << "/20)\n";
    }

    // ══ T3102 张辽行 boss 门：boss 挑战局 0 磨血 ══
    {
        ScenarioBattle b(make_party(mark_pet<351>(/*张辽*/ 2770,
                                                  EffectArgs(std::vector<int>{500, 5}),
                                                  /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        b.ctx().is_boss_challenge = true;
        sbs_srand(11);
        b.start();
        int grinds = 0;
        for (int r = 0; r < 20; ++r) {
            const int before = b.ctx().getPet(1).hp;
            round_actions(b, /*attack=*/true);
            if (before - b.ctx().getPet(1).hp >= 10000) {
                ++grinds;
            }
        }
        CHECK_EQ(grinds, 0, "T3102 351：张辽行（boss无效）→ boss 挑战局 0 磨血");
        std::cout << "  [T3102] 行级 boss 门 通过\n";
    }

    // ══ T3103 旗消费：无使用回合无磨血（防旗残留双跳）══
    {
        ScenarioBattle b(make_party(mark_pet<351>(/*扎尔瑟隆*/ 2729,
                                                  EffectArgs(std::vector<int>{1000, 5}),
                                                  /*speed=*/300)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(7);
        b.start();
        round_actions(b, /*attack=*/true);
        const int foe_after_r1 = b.ctx().getPet(1).hp;
        round_actions(b, /*attack=*/false);   // R2 双 NONE：无使用 → 无磨血
        CHECK_EQ(b.ctx().getPet(1).hp, foe_after_r1,
                 "T3103 351：无攻击技使用回合 → 对手 0 失血（旗不残留）");
        std::cout << "  [T3103] 旗消费 通过\n";
    }

    std::cout << "scenario_3100 PASS (K178 攻击技战阶磨血族)" << std::endl;
    return 0;
}
