#include <utils/sbs_rand.h>
// 场景 3275 —— K194 暴击+吸血族（619，soul_lib/k194_crit_drain.cpp）
//（2026-10-07，worktree ../agents/k194，看板段 3275-3284 → T3275-T3278）
//
// 断言口径：
//   - T3275/T3276 吸血精确（本体 [50,40] / 伙伴行 [60,50]）：对手 NONE、持印者 hp 预置
//     50000 出手 → 回血 = 对手失血 × db[1]/100（K168 3020 同式：DETECT 时点 final 已定、
//     无承伤干扰；暴击与否比例关系不变）。
//   - T3277 单子句行 [30,0]：db[1]=0 → 不装吸血票 → 回血 = 0。
//   - T3278 暴击统计（[50,40]，p=0.5）：20 回合连攻，对手单回失血双峰分离
//     （普通 ≈[0.85,1.0]×raw、暴击 ≈[1.7,2.0]×raw，阈值 45 干净切分）；
//     Binomial(20,0.5) σ≈2.24 → 暴击次数 ∈ [4,16]。
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

constexpr int kPaDa = 10004;   // 拍打：攻击载体
constexpr int kMaxHp = 99999;

template <int MarkId>
ElfPet mark_pet(EffectArgs mark_args, int speed, int hp) {
    ElfPet p = make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, hp,
                        /*soul_mark=*/SoulMark(MarkId, "暴击吸血魂印", "",
                                               std::move(mark_args)));
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

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3275/T3276/T3277 吸血精确（三行参数化）══
    {
        const int rows[3][2] = {{50, 40}, {60, 50}, {30, 0}};
        for (int row = 0; row < 3; ++row) {
            ScenarioBattle b(make_party(mark_pet<619>(EffectArgs(std::vector<int>{
                                                          rows[row][0], rows[row][1]}),
                                                      /*speed=*/300, /*hp=*/kMaxHp)),
                             make_party(plain_foe(/*speed=*/100)));
            sbs_srand(7);
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.seerRobot[0].elfPets[0].hp = 50000;   // 压当前体力留回血空间（max 仍 99999）
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int foe_loss = kMaxHp - ctx.getPet(1).hp;
            const int self_gain = ctx.getPet(0).hp - 50000;
            CHECK(foe_loss > 0, "T3275 前置：攻击落地");
            const int expect = foe_loss * rows[row][1] / 100;
            CHECK_EQ(self_gain, expect,
                     (row == 0 ? "T3275 619：本体行回血 = 伤害×40/100"
                               : (row == 1 ? "T3276 619：伙伴行回血 = 伤害×50/100"
                                           : "T3277 619：db[1]=0 无吸血")));
            std::cout << "  [T327" << row + 5 << "] 吸血" << rows[row][1]
                      << "% 通过 (loss=" << foe_loss << " heal=" << self_gain << ")\n";
        }
    }

    // ══ T3278 暴击统计（20 回合，p=0.5 双峰分离）══
    {
        ScenarioBattle b(make_party(mark_pet<619>(EffectArgs(std::vector<int>{50, 40}),
                                                  /*speed=*/300, /*hp=*/kMaxHp)),
                         make_party(plain_foe(/*speed=*/100)));
        sbs_srand(11);
        b.start();
        int crits = 0;
        for (int r = 0; r < 20; ++r) {
            const int before = b.ctx().getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - b.ctx().getPet(1).hp;
            if (loss >= 45) {
                ++crits;   // 普通 ≈30±4、暴击 ≈60±8：45 干净切分
            }
        }
        CHECK(crits > 0, "T3278 前置：暴击有落地");
        CHECK(crits >= 4 && crits <= 16,
              "T3278 619：20 次攻击暴击次数落 Binomial(20,0.5) 统计带 [4,16]");
        std::cout << "  [T3278] 暴击统计带 通过 (crits=" << crits << "/20)\n";
    }

    std::cout << "scenario_3275 PASS (K194 暴击+吸血族)" << std::endl;
    return 0;
}
