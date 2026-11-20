// 场景 1570 —— J70 每回合窗族（2 宏，moves_lib/j_roundly_window.cpp）
//（2026-10-03，worktree ../agents/j70；号段 1570-1579 → T1570-T1571 两块）
//
// 断言口径：
//   - 载体双属性技（恐怖之潮 25670 / 踏雪荧辉 27829，category 4 零伤害），切片
//     均为 [3,100,异常码,兜底]——概率 100 档：命中路径确定；
//   - 未触发路径 = 对手 R1 用凝神聚力斩 17091 [48]|[4]（4 回合免疫所有异常，atk=1
//     红伤 1~2 无害）→ R2 的 apply 必 TARGET_IMMUNE → 兜底确定触发（概率路径的
//     确定性改造，无需掷歪）；
//   - 1181 兜底 = 对手六槽各 -1（stat_drop 逐槽）；1766 兜底 = 对手全技能 PP-2
//     （pp_reduce 原语逐槽钳 0；基线取 R1 末，对手 R1 自耗 PP 已含在基线内）；
//   - 载体链污染排查：恐怖之潮链上 191=免反票/47=自身免弱票（自侧良性）；踏雪荧辉
//     链上 843=自身先制窗（不碰对手异常/PP 断言面）。
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

constexpr int kPaiDa  = 10004;   // 拍打：物40（探针攻击）
constexpr int kKongBu = 25670;   // 恐怖之潮[191,47,1181]|[4,4,3,100,6,1]：属性技（3回合窗·100%害怕·兜底全属性-1）
constexpr int kTaXue  = 27829;   // 踏雪荧辉[191,1766,843]|[4,3,100,5,2,2,2]：属性技（同门·100%冻伤·兜底PP-2）
constexpr int kNingShen = 17091; // 凝神聚力斩[48]|[4]：物150（4回合免疫所有异常——对手用）

constexpr int kFear   = static_cast<int>(AbnormalStatusId::Fear);     // 害怕=6
constexpr int kFrost  = static_cast<int>(AbnormalStatusId::Frostbite);// 冻伤=5
constexpr int kMaxHp  = 99999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300) {
    ElfPet p = make_pet(1, "己方", skills, speed, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100) {
    ElfPet p = make_pet(2, "对手", skills, speed, 1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

bool levels_all(const BattleContext& ctx, int side, int v) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        if (ctx.ability_levels[side][i] != v) {
            return false;
        }
    }
    return true;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1570 1181 每回合用技 100% 害怕·未触发全属性-1（恐怖之潮 [3,100,6,1]）══
    {
        // a) 命中路径：R2 用技 → 对手害怕，六槽不动
        {
            ScenarioBattle b(make_party(self_pet({kKongBu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 武装窗
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（掷）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear),
                  "T1570.a 1181：R2 用技 100% 害怕落地");
            CHECK(levels_all(ctx, 1, 0), "T1570.a 1181：命中路径无兜底（六槽 0）");
        }
        // b) 兜底路径：对手免疫 → apply 必败 → 六槽各 -1（且不害怕）
        {
            ElfPet me = self_pet({kKongBu, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kNingShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{foe, foe, foe, foe, foe, foe};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 自身武装窗
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 对手开免疫
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFear) == false,
                  "T1570.b 前置：对手免疫中未中害怕");
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（掷必败）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(levels_all(ctx, 1, -1),
                  "T1570.b 1181：未触发兜底全属性-1（六槽各 -1）");
            CHECK(!ctx.has_active_abnormal_status(1, kFear),
                  "T1570.b 1181：免疫期害怕未落地");
        }
        std::cout << "  [T1570] 1181 每回合窗·全属性兜底 通过\n";
    }

    // ══ T1571 1766 每回合用技 100% 冻伤·未触发 PP-2（踏雪荧辉 [3,100,5,2]）══
    {
        // a) 命中路径：R2 用技 → 对手冻伤
        {
            ScenarioBattle b(make_party(self_pet({kTaXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                             make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.has_active_abnormal_status(1, kFrost),
                  "T1571.a 1766：R2 用技 100% 冻伤落地");
        }
        // b) 兜底路径：对手免疫 → 全技能 PP-2
        {
            ElfPet me = self_pet({kTaXue, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            ElfPet foe = foe_pet({kNingShen, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
            std::array<ElfPet, 6> foes{foe, foe, foe, foe, foe, foe};
            ScenarioBattle b(make_party(std::move(me)), std::move(foes));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手开免疫
            b.drive();
            int base[5];
            for (int i = 0; i < 5; ++i) {
                base[i] = ctx.getPet(1).skills[i].pp;
            }
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打（掷必败）
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            for (int i = 0; i < 5; ++i) {
                const int now = ctx.getPet(1).skills[i].pp;
                CHECK_EQ(now, base[i] - 2 < 0 ? 0 : base[i] - 2,
                         "T1571.b 1766：对手槽" + std::to_string(i) + " PP-2（实测 " +
                             std::to_string(now) + "）");
            }
        }
        std::cout << "  [T1571] 1766 每回合窗·PP 兜底 通过\n";
    }

    std::cout << "scenario_1570 PASS (J70 每回合窗族 1181/1766)" << std::endl;
    return 0;
}
