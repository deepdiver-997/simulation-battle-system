#include <utils/sbs_rand.h>
// 场景 190：883 每次命中对手降低对手下只出战精灵最大体力的{0}%，最多降低{1}%
// （2026-09-28，night-3 / D3 王之哈莫 3022；实现 resources/moves_lib/d5_D3.cpp）
//
// 载体：龙神惩戒 31567 = 纯 [883] 切片 [10,40]（隔离单效应用）；
//       王·龙子盛威决 19230 = [760,882,883] 切片 [30,13,52]（本任务真身，集成）。
// T1 命中累积→登场落地：一击击杀（SKILL_EFFECT 先于伤害 → 击杀那击也累积），
//    替补登场吃 10%；在场宠上限不动；登场宠当前体力被同步压到新上限。
// T2 池上限：5 击后池应封在 40%（未封顶会是 60%），第 6 击杀 → 替补 -40%。
// T3 无效不累积：对手先手千秋龙威(884) → 我本回合技能 SKILL_INVALID → 该击不入池
//    （否则 T3 击杀后替补应吃 20% 而非 10%）。
// T4 消费清零：主动换宠也算"下只登场"——B 登场吃 10% 后池清零，C 登场再吃新 10%
//    （不清零则 C 应为 -20%）。
// T5 集成（19230）：4 击封顶 52%（13×4），第 5 击杀 → 替补 -52%。
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kHit = 10004;        // 拍打（物攻 40）
constexpr int kLongShen = 31567;   // 龙神惩戒：[883] [10,40]
constexpr int kQianQiu = 24707;    // 千秋龙威：884 令对手下次使用的技能无效
constexpr int kLongZi = 19230;     // 王·龙子盛威决：[760,882,883] [30,13,52]

int max_hp_of_slot(BattleContext& ctx, int side, int slot) {
    return ctx.seerRobot[side].elfPets[slot].numericalBase[NumericalPropertyIndex::HP];
}

// 攻方：speed 200 先手；must_hit 锁确定性。atk_big=一击必杀档，否则低伤保对手存活。
ElfPet striker(int skill, int atk, bool must_hit) {
    ElfPet p = scenario::make_pet(3022, "攻方", {skill, kHit, kHit, kHit, kHit},
                                  /*speed=*/200, atk, 3000);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = must_hit ? 1 : 0;
    }
    return p;
}

ElfPet target_pet(int speed, int hp, int slot0 = kHit) {
    ElfPet p = scenario::make_pet(1, "守方", {slot0, kHit, kHit, kHit, kHit},
                                  speed, /*atk=*/20, hp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
    }
    return p;
}

// 打一整回合：攻方用 0 号技；op 为守方动作（SELECT_SKILL 或 CHOOSE_PET）。
void round_attack_vs(ScenarioBattle& b, BattleFsm::ActionType op, int index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, op, index);
    b.drive();
}

}  // namespace

int main() {
    sbs_srand(20260928);
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 命中累积 → 替补登场吃 10%（击杀同击也累积） ==========
    {
        constexpr int kBase = 1000;
        ScenarioBattle b(scenario::make_party(striker(kLongShen, 20000, true)),
                         scenario::make_party(target_pet(100, kBase)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // 一击击杀
        CHECK_EQ(max_hp_of_slot(ctx, 1, 0), kBase,
                 "T1 在场宠被击杀，其上限未被 883 触碰");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);  // 替补登场
        const int reduced = max_hp_of_slot(ctx, 1, 1);
        CHECK_EQ(reduced, kBase - kBase / 10, "T1 替补登场吃 10%（击杀那击也累积）");
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, reduced,
                 "T1 登场宠当前体力同步压到新上限");
        std::cout << "  [T1] 命中累积→登场落地通过（" << kBase << " → " << reduced
                  << "）\n";
    }

    //========== T2 池上限 40%：5 击封顶，第 6 击杀 → 替补 -40% 而非 -60% ==========
    {
        constexpr int kBase = 100000;
        ScenarioBattle b(scenario::make_party(striker(kLongShen, 20, true)),
                         scenario::make_party(target_pet(100, kBase)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 5; ++r) {
            round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 1);
        }
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // 击杀
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        const int expected = kBase - kBase * 40 / 100;
        CHECK_EQ(max_hp_of_slot(ctx, 1, 1), expected,
                 "T2 6 击只降 40%：池封顶 {1}%（未封顶会是 60%）");
        std::cout << "  [T2] 池上限通过（替补上限 " << max_hp_of_slot(ctx, 1, 1)
                  << " / 基准 " << kBase << "）\n";
    }

    //========== T3 无效不累积：884 封我技能 → 该击不入池 ==========
    {
        constexpr int kBase = 100000;
        ScenarioBattle b(scenario::make_party(striker(kLongShen, 20, false)),
                         scenario::make_party(target_pet(300, kBase, kQianQiu)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // 守方 0 号=千秋龙威
        CHECK(static_cast<int>(ctx.ws.skill_exec_result[0])
                  == static_cast<int>(SkillExecResult::SKILL_INVALID),
              "T3 前提：我方本击被 884 判无效（SKILL_INVALID）");
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 击杀
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        const int expected = kBase - kBase / 10;
        CHECK_EQ(max_hp_of_slot(ctx, 1, 1), expected,
                 "T3 被封的那击不入池：替补只吃 R2 一击的 10%（错则 20%）");
        std::cout << "  [T3] 无效不累积通过\n";
    }

    //========== T4 消费清零：主动换宠 B 吃 10% 后，C 登场再吃新 10% ==========
    {
        constexpr int kBase = 100000;
        ScenarioBattle b(scenario::make_party(striker(kLongShen, 20, true)),
                         scenario::make_party(target_pet(100, kBase)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R1 池 10
        round_attack_vs(b, BattleFsm::ActionType::CHOOSE_PET, 1);     // R2 B 登场吃 10，池清
        CHECK_EQ(max_hp_of_slot(ctx, 1, 1), kBase - kBase / 10, "T4 B 登场吃 10%");
        round_attack_vs(b, BattleFsm::ActionType::CHOOSE_PET, 2);     // R3 C 登场吃新 10
        CHECK_EQ(max_hp_of_slot(ctx, 1, 2), kBase - kBase / 10,
                 "T4 消费清零：C 只吃新一轮 10%（不清零会是 20%）");
        CHECK_EQ(max_hp_of_slot(ctx, 1, 1), kBase - kBase / 10, "T4 B 的削减保持");
        std::cout << "  [T4] 消费清零通过（B/C 上限均 " << max_hp_of_slot(ctx, 1, 2)
                  << "）\n";
    }

    //========== T5 集成（王·龙子盛威决 19230）：4 击封顶 52%，第 5 击杀 ==========
    {
        constexpr int kBase = 100000;
        ScenarioBattle b(scenario::make_party(striker(kLongZi, 20, true)),
                         scenario::make_party(target_pet(100, kBase)));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int r = 0; r < 4; ++r) {
            round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        }
        ctx.seerRobot[1].elfPets[ctx.on_stage[1]].hp = 1;
        round_attack_vs(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // 第 5 击杀（pp 恰好 5）
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        const int expected = kBase - kBase * 52 / 100;
        CHECK_EQ(max_hp_of_slot(ctx, 1, 1), expected,
                 "T5 19230 切片 [13,52]：4 击封顶 52%，第 5 击不超限");
        CHECK_EQ(ctx.seerRobot[1].elfPets[1].hp, max_hp_of_slot(ctx, 1, 1),
                 "T5 登场宠当前体力压到新上限");
        std::cout << "  [T5] 19230 集成通过（替补上限 " << max_hp_of_slot(ctx, 1, 1)
                  << " / 基准 " << kBase << "）\n";
    }

    std::cout << "scenario_190 all passed" << std::endl;
    return 0;
}
