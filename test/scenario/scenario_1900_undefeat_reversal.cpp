// 场景 1900 —— J99 未击败固伤/反转回满族（2 宏，moves_lib/j_undefeat_reversal.cpp）
//（2026-10-03，worktree ../agents/j99；号段 1900-1909 → T1900-T1901 两块）
//
// 断言口径：
//   - 838（萌神巨力击 [200] 特150 纯链）：R1 载体攻击（atk=1 对手存活 → 门开）→
//     R2 ROUND_START 附加 200 精确（双方 NONE，无杂散）；对照 = 自身 atk=1500
//     一击击败（换宠登场满血）→ 门关 → 新宠无附加；
//   - 778（天降魔神 [778] 物80 纯链）：对手预置 +2 提升两槽 → 反转 REVERSED
//     （+2 翻 -2）+ 自身回满 5000；对照 = 对手无提升 → 反转不成立 → 不回。
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
constexpr int kMengShen = 18208; // 萌神巨力击 [838]|[200]：特150（未击败固伤载体）
constexpr int kTianJiang = 17753; // 天降魔神 [778]：物80（反转回满载体）
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

    // ══ T1900 838 未击败 → 下回合 +200（萌神巨力击 [200]）══
    {
        // a) R1 攻击（对手存活 → 门开）→ R2 开场 +200（双方 NONE 无杂散）
        {
            ScenarioBattle b(
                make_party(mk_self({kMengShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 载体
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int h1 = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 双 NONE
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(h1 - ctx.getPet(1).hp, 200,
                     "T1900.a 838：下回合开场附加 200（实测 " +
                         std::to_string(h1 - ctx.getPet(1).hp) + "）");
        }
        // b) 对照：R1 一击击败（换宠登场满血）→ 门关 → 无附加
        {
            ScenarioBattle b(
                make_party(mk_self({kMengShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa}, /*atk=*/1500)),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            for (int slot = 0; slot < 6; ++slot) {
                ctx.seerRobot[1].elfPets[slot].hp = 500;   // 全队低血：一击可败
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 击败
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 0,
                  "T1900.b 前置：对手首宠已被击败（等替换选择）");
            b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);     // 对手选替换宠
            b.drive();
            CHECK_EQ(ctx.on_stage[1], 1, "T1900.b 前置：替换宠已登场");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);           // R2 双 NONE
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(1).hp, 500,
                  "T1900.b 838 对照：击败门关 → 新宠无附加（实测 " +
                      std::to_string(ctx.getPet(1).hp) + "）");
        }
        std::cout << "  [T1900] 838 未击败固伤 通过\n";
    }

    // ══ T1901 778 反转成功 → 回满（天降魔神 [778]）══
    {
        // a) 对手 +2 两槽 → REVERSED（翻 -2）→ 自身回满
        {
            ScenarioBattle b(
                make_party(mk_self({kTianJiang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            (void)stat_change(&ctx, 1, 0, 2);          // 对手攻击 +2
            (void)stat_change(&ctx, 1, 3, 2);          // 特防 +2
            ctx.getPet(0).hp = 3000;                   // 预置缺口
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.ability_levels[1][0], -2,
                     "T1901.a 前置：对手提升被反转（槽0 实测 " +
                         std::to_string(ctx.ability_levels[1][0]) + "）");
            CHECK_EQ(ctx.getPet(0).hp, kMaxHp,
                     "T1901.a 778：反转成功 → 回满（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        // b) 对照：对手无提升 → 反转不成立 → 不回
        {
            ScenarioBattle b(
                make_party(mk_self({kTianJiang, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 3000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 3000,
                     "T1901.b 778 对照：无提升不反转不回（实测 " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T1901] 778 反转回满 通过\n";
    }

    std::cout << "scenario_1900 PASS (J99 未击败固伤/反转回满族 838/778)" << std::endl;
    return 0;
}
