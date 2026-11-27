#include <utils/sbs_rand.h>
// 场景 170：湮灭之主·咤克斯 4762 第五技 —— 绝灭·万物哀鸣 37914
//   效果 2295（args_num=0）+ 698（[100,23]）（moves_lib/d_fifth_skills.cpp，2026-09-28）
//
// 2295："废除此技能，恢复对方精灵的全部体力，消耗此技能的全部PP值并获得等量的魔王
//        咒怨，然后若魔王咒怨层数高于5则秒杀对手且造成的攻击伤害不低于对方精灵体力
//        上限之和"
// 698："当回合击败对手时，对方下只精灵出战时{0}%{1}"（携带参数 [100,23] = 100% 诅咒）
//
// 断言组：
//   T1 2295 主路径（0 垫底 → 5 层，不 >5）：废除 is_locked + 对方回满 + PP 5→0
//      + 咒怨 +5 层 + 对手存活。
//      ★ "5 层"是顺序敏感断言：PP 消耗计的是**使用前剩余 PP**（SKILL_EFFECT 时点
//        早于引擎 AFTER_ACTION 的扣 1 点）——若时点颠倒会只得 4 层。
//   T2 2295 秒杀档（1 垫底 → 6 > 5）：对手倒地（FLOOR 保底伤害或 AFTER_ACTION
//      force_hp_to_zero 补刀，两者任一致死）+ 层数 6。
//   T3 698 主路径：T2 同场击败 → 对方死亡换宠 → 新出战精灵 100% 诅咒（23）。
//   T4 698 反例：未击败（5 层不秒）时对方主动换宠 → 新出战精灵不被诅咒（"击败"
//      前提不成立，pending 从未置位）。
//   T5 锁定口径：2295 废除后 query_selectable = LOCKED（§7.6 选择锁，第五解封线）。
//
// 确定性：暴击率 0、必中（37914 must_hit=1 自带）；srand 调种控咒怨魂印共享层的
// 3%/层 FLOOR 掷点（T1 5 层 = 15% 掷点需不中，否则保底会打死对手）。
#include <cstdlib>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include "../../resources/curse_stack.h"
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

using scenario::ScenarioBattle;

constexpr int kJueMie = 37914;    // 绝灭·万物哀鸣：混沌特攻 先1 必中 [2295, 698]
constexpr int kHarmless = 10938;  // 本能一击：威力 0
constexpr int kCurse = static_cast<int>(AbnormalStatusId::Curse);  // 23 诅咒

ElfPet zha(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(0, "湮灭之主·咤克斯", skills, /*speed=*/200, /*atk=*/300,
                                  /*hp=*/5000);
    return p;
}

ElfPet foe(std::array<int, 5> skills) {
    ElfPet p = scenario::make_pet(1, "对手", skills, /*speed=*/100, /*atk=*/50,
                                  /*hp=*/100000);
    return p;
}

void use_fifth(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 4);   // 绝灭·万物哀鸣
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== T1 2295 主路径：废除 + 回满对方 + PP→5 层咒怨 + 不秒杀 ==========
    {
        // 经验调种：咒怨共享层 FLOOR 掷点（5 层 = rand%100 < 15 需不中）。
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(zha({kHarmless, kHarmless, kHarmless, kHarmless, kJueMie})),
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(1).hp = 30000;   // 预伤：给"恢复对方全部体力"一个可观察的起点
        use_fifth(b);
        CHECK(ctx.getPet(0).skills[4].is_locked, "T1 核心：废除此技能 → is_locked");
        CHECK_EQ(ctx.getPet(0).skills[4].pp, 0, "T1 核心：消耗此技能的全部PP值（5→0）");
        CHECK_EQ(get_curse_stacks_slot(&ctx, 0, ctx.on_stage[0]), 5,
                 "T1 核心：获得等量魔王咒怨（= 使用前剩余 PP 5；★ 时点锁：早于 AFTER_ACTION 扣 1）");
        CHECK(ctx.getPet(1).hp >= 99999,
              "T1 核心：恢复对方精灵的全部体力（30000→99999+；再吃本次威力 0 攻击的最低伤害 1 点）");
        CHECK(ctx.getPet(1).hp > 0, "T1 5 层不高于 5 → 不秒杀");
        std::cout << "  [T1] 2295 主路径（废除+回满+PP→5层）通过\n";
    }

    //========== T2+T3 2295 秒杀档 + 698 击败→对方下只出战 100% 诅咒 ==========
    {
        sbs_srand(11);
        ScenarioBattle b(scenario::make_party(zha({kHarmless, kHarmless, kHarmless, kHarmless, kJueMie})),
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        // 垫底 1 层（模拟魂印途径已获层）→ 本技 +5 = 6 > 5 → 秒杀
        add_curse_stacks_slot(&ctx, 0, ctx.on_stage[0], 1);
        use_fifth(b);
        CHECK(ctx.getPet(1).hp <= 0, "T2 核心：6 > 5 层 → 秒杀对手（保底伤害或补刀致死）");
        CHECK_EQ(get_curse_stacks_slot(&ctx, 0, ctx.on_stage[0]), 6, "T2 层数 1+5=6");
        CHECK(ctx.getPet(0).skills[4].is_locked, "T2 前提：本技同样废除");

        // T3：对方死亡换宠 → 新出战精灵 100% 诅咒（698；归因=last_damage_actor 本方）
        REQUIRE(ctx.currentState == State::CHOOSE_AFTER_DEATH);   // T3 前置：对方倒地等补位
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 1, "T3 前置：对方下只精灵已出战");
        CHECK(!ctx.getPet(1).skills[0].is_locked, "T3 前置：新出战精灵本体干净");
        CHECK(ctx.has_active_abnormal_status(1, kCurse),
              "T3 核心：对方下只精灵出战时 100% 进入诅咒状态（23）");
        std::cout << "  [T2/T3] 2295 秒杀档 + 698 下只出战诅咒 通过\n";
    }

    //========== T4 698 反例：未击败时对方换宠 → 不诅咒 ==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(zha({kHarmless, kHarmless, kHarmless, kHarmless, kJueMie})),
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        use_fifth(b);   // 5 层：不击败（pending 从未置位）
        CHECK(ctx.getPet(1).hp > 0, "T4 前置：对手存活");
        // 双方纯换宠（无攻击 → 无击败可能；也不受咒怨 3%/层保底掷点扰动）
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        CHECK_EQ(ctx.on_stage[1], 1, "T4 前置：对方已换宠");
        CHECK(ctx.getPet(1).hp > 0, "T4 前置：新出战精灵满血在场");
        CHECK(!ctx.has_active_abnormal_status(1, kCurse),
              "T4 核心：未击败 → 对方下只出战精灵不被诅咒");
        std::cout << "  [T4] 698 未击败反例 通过\n";
    }

    //========== T5 锁定口径：废除后选择锁（query_selectable = LOCKED） ==========
    {
        sbs_srand(7);
        ScenarioBattle b(scenario::make_party(zha({kHarmless, kHarmless, kHarmless, kHarmless, kJueMie})),
                         scenario::make_party(foe({kHarmless, kHarmless, kHarmless, kHarmless, kHarmless})));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(ctx.getPet(0).skills[4].query_selectable(&ctx, 0) != SkillSelectionResult::LOCKED,
              "T5 前置：使用前第五技可选");
        use_fifth(b);
        CHECK_EQ(ctx.getPet(0).skills[4].query_selectable(&ctx, 0), SkillSelectionResult::LOCKED,
                 "T5 核心：废除后 = 选择锁（§7.6：本体不可用 → 不可选）");
        std::cout << "  [T5] 2295 废除→选择锁 通过\n";
    }

    std::cout << "\n场景 170 通过：咤克斯 4762 绝灭·万物哀鸣 2295+698" << std::endl;
    return 0;
}
