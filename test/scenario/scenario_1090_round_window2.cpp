// 场景 1090 —— J24 回合窗响应族二（535/153/169，moves_lib/j_round_window2.cpp）
//（2026-10-02，worktree ../agents/j24；号段 1090-1099 → T1090-T1092 三块）
//
// 断言口径：
//   - 535 防/特防逐次 +1 精确等式（受击触发、属性技不触发对照）；
//   - 153 吸血走 **对拍整数断言**：同回合内 H（自体回量）== D（对敌红伤）/2 向下取整，
//     即 H*2 - D ∈ {0,1}；窗口过期回合 H==0；R1 留缺口防 heal clamp；
//   - 169 用 chance=100 载体（火之奥义 [3,100,2]）：InRounds 含当回合 → 施放回合即烧伤
//     （169 与 592 的关键语义差）；atk=1 消伤害噪声。
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

// ── 载体技（切片已逐条与 side_effect/arg 核对）──
constexpr int kPaiDa   = 10004;   // 拍打：物40（攻击探针）
constexpr int kShenZi  = 22736;   // 神子之威 [5,5,535]|[…,3] 属（535 3回合窗）
constexpr int kXieXue  = 21414;   // 喋血之魂 [153]|[2,2]        属（153 2回合 1/2）
constexpr int kHuoZhi  = 22075;   // 火之奥义 [48,169]|[5,3,100,2] 属（169 100%烧伤）
constexpr int kGaoSu   = 20017;   // 高速移动（effect 4 属性技，对手触发器）

constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);   // 2

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1, int hp = 99999) {
    ElfPet p = make_pet(1, "窗族二", skills, speed, atk, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1, int hp = 99999) {
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

    // ══ T1090 535 受击→防+1特防+1（22736 [3]）══
    {
        {   // (a) R1 对手攻击 → 防+1/特防+1；R2 再受击 → +2/+2（逐次触发）
            ScenarioBattle b(make_party(self_pet({kShenZi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/100, /*atk=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 神子之威
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手攻击
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 1, "T1090.a 535：受击 → 防御 +1");
            CHECK_EQ(ctx.ability_levels[0][3], 1, "T1090.a 535：受击 → 特防 +1");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 2, "T1090.a 535：R2 再受击 → 防 +2（逐次）");
            CHECK_EQ(ctx.ability_levels[0][3], 2, "T1090.a 535：R2 再受击 → 特防 +2");
        }
        {   // (b) 对照：对手用属性技（不构成"受攻击"）→ 不触发
            ScenarioBattle b(make_party(self_pet({kShenZi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kGaoSu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手属性技
            b.drive();
            CHECK_EQ(ctx.ability_levels[0][2], 0, "T1090.b 535 对照：属性技不触发（防）");
            CHECK_EQ(ctx.ability_levels[0][3], 0, "T1090.b 535 对照：属性技不触发（特防）");
        }
        std::cout << "  [T1090] 535 受击双防+对照 通过\n";
    }

    // ══ T1091 153 每回合对敌伤害 1/2 回自体（21414 [2,2]）══
    {
        ScenarioBattle b(make_party(self_pet({kPaiDa, kXieXue, kPaiDa, kPaiDa, kPaiDa},
                                             /*speed=*/300, /*atk=*/1000, /*hp=*/5000)),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                            /*speed=*/100, /*atk=*/3000)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 喋血之魂（开窗）
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手打我留缺口
        b.drive();
        const int my_hp_r1 = ctx.getPet(0).hp;
        const int foe_hp_r1 = ctx.getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R2 拍打（造成伤害）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int dealt = foe_hp_r1 - ctx.getPet(1).hp;   // R2 对敌红伤
        const int healed = ctx.getPet(0).hp - my_hp_r1;   // R2 自体回量
        CHECK(dealt > 0, "T1091 前置：R2 对敌造成伤害");
        CHECK(healed * 2 >= dealt - 1 && healed * 2 <= dealt,
              "T1091 153：回量=伤害 1/2 向下取整（dealt=" + std::to_string(dealt) +
                  " healed=" + std::to_string(healed) + "）");
        const int my_hp_r2 = ctx.getPet(0).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R3 拍打（窗 [R1,R2] 已过）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(ctx.getPet(0).hp, my_hp_r2,
                 "T1091 153：R3 窗口过期不再回血（掉血仅对手无、自体无回）");
        std::cout << "  [T1091] 153 对敌吸血窗 通过 (dealt=" << dealt << " healed=" << healed
                  << ")\n";
    }

    // ══ T1092 169 每回合用技异常（22075 [3,100,2]，InRounds 含当回合）══
    {
        ScenarioBattle b(make_party(self_pet({kHuoZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 火之奥义（施放即"用技"）
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kBurn),
              "T1092 169：InRounds 含当回合 → 施放回合即 100% 烧伤");
        std::cout << "  [T1092] 169 用技异常窗 通过\n";
    }

    std::cout << "scenario_1090 PASS (J24 回合窗响应族二)" << std::endl;
    return 0;
}
