#include <utils/sbs_rand.h>
// 场景 3070 —— K173 先/后手双分支族·护盾版（809，soul_lib/k173_order_branch_shield.cpp）
//（2026-10-07，worktree ../agents/k173，看板段 3070-3079 → T3070-T3073）
//
// 断言口径：
//   - T3070 先手分支（对照对局对，同 seed 同操作——伤害浮动序列一致）：
//     持印者先手出手 → 对手紧接着的当次技能被 SEAL_ALL 无效 → 持印者失血 = 0、
//     对手照常吃到持印者的伤害；对照局（无印）双方照常互伤。先手分支**不给盾**。
//   - T3071 后手分支（本体行 [200,1,2,1]）：后手回合末 +200 盾（shield_bank_.total）；
//     次回合先制+2 生效 → 低速翻盘先手（preemptive_right）→ 先手分支又封对手当次技
//     → 次回合持印者继续 0 失血、盾未被消耗（对手没打出伤害）。
//   - T3072 进化行 [250,1,2,1]：同 T3071 首回合断言盾 = 250（行参驱动数值）。
//   - T3073（BOSS无效）：is_boss_challenge=true → 节点不注册不执行 → 后手回合末无盾。
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

template <int MarkId>
ElfPet mark_pet(const std::array<int, 5>& skills, int speed, int speed_unused_hp,
                EffectArgs mark_args) {
    (void)speed_unused_hp;
    ElfPet p = make_pet(1, "卡修斯", skills, speed, /*atk=*/100, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(MarkId, "王·卡修斯魂印", "",
                                               std::move(mark_args)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_me(int speed) {
    ElfPet p = make_pet(1, "对照", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/100, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_foe(int speed) {
    ElfPet p = make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, speed,
                        /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void both_attack(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T3070 先手分支：对手当回合技能无效（对照对局对）══
    {
        int marked_self_loss = -1;
        int marked_foe_loss = -1;
        int plain_self_loss = -1;
        int plain_foe_loss = -1;
        for (int side = 0; side < 2; ++side) {
            auto me = side == 0
                ? mark_pet<809>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa}, /*speed=*/300, 0,
                                EffectArgs(std::vector<int>{200, 1, 2, 1}))
                : plain_me(/*speed=*/300);
            ScenarioBattle b(make_party(std::move(me)),
                             make_party(plain_foe(/*speed=*/100)));
            sbs_srand(7);
            b.start();
            both_attack(b);
            BattleContext& ctx = b.ctx();
            const int self_loss = 99999 - ctx.getPet(0).hp;
            const int foe_loss = 99999 - ctx.getPet(1).hp;
            if (side == 0) {
                marked_self_loss = self_loss;
                marked_foe_loss = foe_loss;
                CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0,
                         "T3070 809：先手分支不给盾");
            } else {
                plain_self_loss = self_loss;
                plain_foe_loss = foe_loss;
            }
        }
        CHECK(marked_foe_loss > 0, "T3070 前置：持印者先手伤害落地");
        CHECK_EQ(marked_self_loss, 0, "T3070 809：先手→对手当次技无效（持印者 0 失血）");
        CHECK(plain_self_loss > 0, "T3070 对照：无印时对手伤害照常落地");
        CHECK_EQ(plain_foe_loss, marked_foe_loss,
                 "T3070 对照：先手方伤害序列不受印影响（同 seed 同值）");
        std::cout << "  [T3070] 先手封技 通过 (self_loss=" << marked_self_loss
                  << " foe_loss=" << marked_foe_loss << " 对照self=" << plain_self_loss
                  << ")\n";
    }

    // ══ T3071 后手分支：回合末+200 盾 → 次回合先制+2 翻盘先手（本体行）══
    {
        ScenarioBattle b(make_party(mark_pet<809>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  /*speed=*/100, 0,
                                                  EffectArgs(std::vector<int>{200, 1, 2, 1}))),
                         make_party(plain_foe(/*speed=*/300)));
        sbs_srand(7);
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T3071 前置：开场无盾");
        both_attack(b);   // R1：对手先手，持印者后手
        CHECK(ctx.getPet(0).hp < 99999, "T3071 前置：R1 后手方照常吃到先手伤害");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 200,
                 "T3071 809：后手→回合末 +200 盾（本体行 db[0]=200）");
        const int hp_after_r1 = ctx.getPet(0).hp;
        both_attack(b);   // R2：先制+2 → 低速持印者翻盘先手
        CHECK(ctx.preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3071 809：次回合先制+2 → 低速持印者先手");
        CHECK_EQ(ctx.getPet(0).hp, hp_after_r1,
                 "T3071 809：R2 先手分支封住对手当次技 → 持印者 0 失血");
        CHECK(ctx.getPet(0).shield_bank_.total() == 200,
              "T3071 809：R2 对手被封 → 盾未被消耗");
        std::cout << "  [T3071] 后手盾+次回合先制 通过\n";
    }

    // ══ T3072 进化行 [250,1,2,1]：盾量随行参 ══
    {
        ScenarioBattle b(make_party(mark_pet<809>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  /*speed=*/100, 0,
                                                  EffectArgs(std::vector<int>{250, 1, 2, 1}))),
                         make_party(plain_foe(/*speed=*/300)));
        sbs_srand(7);
        b.start();
        both_attack(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 250,
                 "T3072 809：进化行 db[0]=250 → 盾 250");
        std::cout << "  [T3072] 进化行盾量 通过\n";
    }

    // ══ T3073（BOSS无效）：boss 挑战局节点不注册不执行 ══
    {
        ScenarioBattle b(make_party(mark_pet<809>({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                                                  /*speed=*/100, 0,
                                                  EffectArgs(std::vector<int>{200, 1, 2, 1}))),
                         make_party(plain_foe(/*speed=*/300)));
        b.ctx().is_boss_challenge = true;
        sbs_srand(7);
        b.start();
        both_attack(b);
        CHECK_EQ(b.ctx().getPet(0).shield_bank_.total(), 0,
                 "T3073 809：boss 挑战局（BOSS无效）→ 后手不给盾");
        std::cout << "  [T3073] BOSS无效 通过\n";
    }

    std::cout << "scenario_3070 PASS (K173 先/后手双分支族·护盾版)" << std::endl;
    return 0;
}
