// 场景 2081 —— J114 先出手门族二·条件宏（680/528/1467，
// moves_lib/j_first_move_gates2.cpp；2026-10-03，worktree ../agents/j114）
//
// 断言口径：
//   - 680 先手概率异常：悬冰伏徒 37262 [100,15,2]（100% 冰封 2回合，确定档）
//       (a) 先手 → foe 冰封落地；(b) 后手对照 → 不落地（先短路后掷骰）。
//   - 528 先手免弱窗：自动攻击 31196 [2]（2回合 STAT_DROP 票）+ foe 恐惧尖叫 11628
//     [5,5]|[1,100,-1,3,100,-1]（各 100% 降对手特攻/特防 1，纯链；⚠️ 效果 4 是"自身属性变化"，
//     降对手必须用效果 5 载体——飞叶风暴 10017 实为自降，不能当降对手工具刀）：
//       (a) R1 用 528 → R1/R2 foe 降不落（无负槽）、R3 窗过期落地（恰一槽 -2）；
//       (b) 对照（不用 528）→ R1 即落地。
//   - 1467 随机强化+先手翻倍：衍生怨灵 27989 [3,1]（属，无伤害）
//       (a) 先手 → 恰 3 槽 +2；(b) 后手 → 恰 3 槽 +1。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打（对照刀/填充）
constexpr int kXuanBing = 37262;  // 悬冰伏徒 [680,499,1478]|[100,15,2,2,1]（100% 冰封）
constexpr int kZiDong  = 31196;   // 自动攻击 [528]|[2]（纯链 528 载体）
constexpr int kKongBu  = 11628;   // 恐惧尖叫 [5,5]|[3,100,-1,3,100,-1]（双 100% 降对手）
constexpr int kYanSheng = 27989;  // 衍生怨灵 [1467]|[3,1]（纯链属，随机强化）

constexpr int kIcebound = static_cast<int>(AbnormalStatusId::Icebound);   // 15

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "条件门", skills, speed, /*atk=*/1, 5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, /*atk=*/1, 99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int negative_slots(const BattleContext& ctx, int side) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] < 0) {
            ++n;
        }
    }
    return n;
}

int count_slots_at(const BattleContext& ctx, int side, int value) {
    int n = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] == value) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2082 680 先手概率异常（100% 确定档）══
    {
        {   // (a) 先手 → 冰封落地
            ScenarioBattle b(make_party(self_pet({kXuanBing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kIcebound),
                  "T2082.a 680 先手：100% 冰封落地");
        }
        {   // (b) 后手对照 → 不落地
            ScenarioBattle b(make_party(self_pet({kXuanBing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/50)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(!ctx.has_active_abnormal_status(1, kIcebound),
                  "T2082.b 680 后手对照：不落地");
        }
        std::cout << "  [T2082] 680 先手异常+对照 通过\n";
    }

    // ══ T2083 528 先手免弱窗（2回合）══
    {
        {   // (a) R1 用 528 → R1/R2 降不落，R3 落地
            ScenarioBattle b(make_party(self_pet({kZiDong, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kKongBu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 自动攻击（528）
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 恐惧尖叫
            b.drive();
            CHECK_EQ(negative_slots(ctx, 0), 0, "T2083.a R1 免弱窗内：无负槽（实测 " +
                                                     std::to_string(negative_slots(ctx, 0)) +
                                                     "）");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(negative_slots(ctx, 0), 0, "T2083.a R2 窗内（2回合含次回合）：无负槽");
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(negative_slots(ctx, 0), 2, "T2083.a R3 窗过期：恰两槽 -1（实测 " +
                                                    std::to_string(negative_slots(ctx, 0)) +
                                                    "）");
        }
        {   // (b) 对照：R1 不用 528 → 即落地
            ScenarioBattle b(make_party(self_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kKongBu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(negative_slots(ctx, 0), 2, "T2083.b 对照：R1 即落两槽 -1");
        }
        std::cout << "  [T2083] 528 先手免弱窗+对照 通过\n";
    }

    // ══ T2084 1467 随机强化+先手翻倍 ══
    {
        {   // (a) 先手：恰 3 槽 +2
            ScenarioBattle b(make_party(self_pet({kYanSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_slots_at(ctx, 0, 2), 3, "T2084.a 1467 先手翻倍：恰 3 槽 +2（实测 " +
                                                       std::to_string(count_slots_at(ctx, 0, 2)) +
                                                       "）");
            CHECK_EQ(count_slots_at(ctx, 0, 1), 0, "T2084.a 无 +1 槽（全为 +2 或 0）");
        }
        {   // (b) 后手：恰 3 槽 +1
            ScenarioBattle b(make_party(self_pet({kYanSheng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                 /*speed=*/50)),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                                /*speed=*/400)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(count_slots_at(ctx, 0, 1), 3, "T2084.b 1467 后手：恰 3 槽 +1（实测 " +
                                                       std::to_string(count_slots_at(ctx, 0, 1)) +
                                                       "）");
            CHECK_EQ(count_slots_at(ctx, 0, 2), 0, "T2084.b 无 +2 槽（未翻倍）");
        }
        std::cout << "  [T2084] 1467 随机强化翻倍+对照 通过\n";
    }

    std::cout << "scenario_2081 ALL PASS" << std::endl;
    return 0;
}
