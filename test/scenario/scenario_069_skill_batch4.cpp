#include <utils/sbs_rand.h>
// 场景 069：技能批 4 —— 随机回血 / 全属性提升 / 全属性弱化（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）+ 图鉴原文核对：
//   - 450 随机恢复{0}到{1}点生命值（幸运百宝袋 22129：args [100,500]）
//   - 585 "技能使用成功时，{0}"——六项属性各带等级的提升模板。⚠️ 官方参数序是第三套
//     码序 [攻,防,特攻,特防,速,命中]（new_se Eid 34 序 + 命中）：雷皇现世 23360
//     args [1,1,0,1,1,1] ↔ 图鉴「攻击防御特防速度命中+1」——特攻跳过、0 落在第 3 位。
//   - 749 {0}%令对手全属性-{1}（狂化电流 24009：args [100,1]）——概率门掷一次，
//     命中后 5 项战斗属性逐项 stat_drop（各自查免弱、各自钳 -6）。
//
// 验证点（代表招全部必中）：
//   S1 585：六项等级 == [1, 0, 1, 1, 1, 1]（钉死参数序，特攻位跳过）
//   H1 450：回血落在 [lo,hi] 区间；残血不足时被钳满
//   D1 749：100% → 对手 5 项全 -1；免弱窗口 → 一点不动
#include <cassert>
#include <cstdlib>
#include <iostream>

#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

namespace {

constexpr int kPaiDa = 10004;      // 拍打：物攻 40
constexpr int kGaoSu = 20017;      // 高速移动：属性技（无伤害无还手）
constexpr int k585 = 23360;        // 雷皇现世：[585,57]（acc 100 必中）
constexpr int k450 = 22129;        // 幸运百宝袋：[450] args[100,500]（acc 100 必中）
constexpr int k749 = 24009;        // 狂化电流：[749,433] args[100,1,…]（acc 100 必中）

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1,
                           const MedicineStock& meds0 = {},
                           const MedicineStock& meds1 = {}) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1), meds0, meds1);
}

void round_of(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

ElfPet attacker(int skill) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill}, 200, 200, 4000);
}
ElfPet quiet_defender(int speed = 100) {
    return scenario::make_pet(1, "守方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                              speed, 100, 4000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== S1 effect 585：六项属性提升（钉死官方参数序）==========
    {
        ScenarioBattle b = make_battle(attacker(k585), quiet_defender());
        b.start();
        round_of(b);
        // 官方序 [攻,防,特攻,特防,速,命中] = [1,1,0,1,1,1] → 引擎序期望：
        // 攻+1、特攻 0（跳过）、防+1、特防+1、速+1、命中+1
        const int expect[6] = {1, 0, 1, 1, 1, 1};
        for (int i = 0; i < 6; ++i) {
            char msg[80];
            std::snprintf(msg, sizeof(msg), "S1) 引擎 stat[%d] 应为 %+d（官方序已映射）", i,
                          expect[i]);
            CHECK_EQ(b.ctx().ability_levels[0][i], expect[i], msg);
        }
        std::cout << "S1 PASS 585 六项提升（引擎序 [1,0,1,1,1,1]，官方码序已映射）\n";
    }

    //========== H1 effect 450：随机回血 ==========
    {
        ScenarioBattle b = make_battle(attacker(k450), quiet_defender());
        b.start();
        b.pet(0).hp = 1000;
        round_of(b);
        const int healed = b.pet(0).hp - 1000;
        CHECK(healed >= 100 && healed <= 500,
              "H1) 回血应落在 [100,500]（实际 " + std::to_string(healed) + "）");
        std::cout << "H1a PASS 450 区间回血（回 " << healed << "）\n";

        // 残血 3900（缺口 100）→ 任何掷点都被钳满
        ScenarioBattle b2 = make_battle(attacker(k450), quiet_defender());
        b2.start();
        b2.pet(0).hp = 3900;
        round_of(b2);
        CHECK_EQ(b2.pet(0).hp, 4000, "H1) 缺口小于最小回血时应钳满");
        std::cout << "H1b PASS 缺口不足时钳满\n";
    }

    //========== D1 effect 749：概率全属性弱化 ==========
    {
        // 守方嗑药（满血回血钳 0）跳过出手——避免高速移动的 +2 速度污染 -1 断言
        const MedicineStock meds1 = {{300011, 3}};   // 3 瓶初级体力药剂(+20体力)，满血嗑=钳0跳过出手
        ScenarioBattle b = make_battle(attacker(k749), quiet_defender(), {}, meds1);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 狂化电流 [100,1]
        b.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b.drive();
        for (int i = 0; i < 5; ++i) {
            char msg[64];
            std::snprintf(msg, sizeof(msg), "D1) 对手 stat[%d] 应 -1", i);
            CHECK_EQ(b.ctx().ability_levels[1][i], -1, msg);
        }
        std::cout << "D1a PASS 749 全属性 -1\n";

        // 免弱窗口 → 一点不动
        ScenarioBattle b2 = make_battle(attacker(k749), quiet_defender(), {}, meds1);
        b2.start();
        b2.ctx().grant_immunity(/*owner=*/1, ImmunityType::STAT_DROP, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/3,
                                /*source_id=*/999069, /*soul_immunity=*/false,
                                EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/999069);
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::USE_MEDICINE, 0);
        b2.drive();
        for (int i = 0; i < 5; ++i) {
            CHECK_EQ(b2.ctx().ability_levels[1][i], 0, "D1) 免弱时等级一点不动");
        }
        std::cout << "D1b PASS 免弱挡住全属性弱化\n";
    }

    std::cout << "\n场景 069 全部通过：技能批 4（450/585/749）✓" << std::endl;
    return 0;
}
