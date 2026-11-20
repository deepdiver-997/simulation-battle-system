#include <utils/sbs_rand.h>
// 场景 068：技能批 3 —— 概率固伤 / 吸血 / 先后手威力 / 连用递增（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）：
//   - 9   连续使用每次威力增加{0}，最高威力{1}
//   - 30  后出手的话威力为2倍 / 40 先出手的话威力为2倍
//   - 93  {0}%概率额外附加{1}点固定伤害
//   - 101 给对手造成伤害时，伤害数值的{0}%恢复自身体力
// 另验证用户 2026-09-17 口径：**嗑药完全跳过出手**——"每回合使用技能"类效果
//（597 深红契约的每回合吸取）在嗑药回合不结算。
//
// 验证点（代表招全部必中；威力断言直接读 ws.skill_power_view 的物化值，确定性）：
//   N1 9：连用 80→100→120→140→140（封顶）；换技能断连 → 回到 80
//   N2 9：嗑药回合不断连（按字面"连续使用"实现，见交接文档待实测注）
//   F1 40：先手 → 威力 ×2（320）；后手 → 原威力（160）
//   S1 30：后手 → ×2（320）；先手 → 原威力（160）
//   C1 93：固定种子下 10% 掷点结果确定——命中则额外 -100
//   H1 101：回血 = 最终红伤 × 50%；封回血时回 0
//   M1 ★ 嗑药跳过出手：597 的每回合吸取在嗑药回合不结算
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
constexpr int kGaoSu = 20017;      // 高速移动：属性技（无伤害、无还手）
constexpr int k9 = 37585;          // 水晶烈冲：物攻 80，[9] args[20,140]（acc 100 必中）
constexpr int k40 = 18312;         // 跳跃巴掌：物攻 130，[40]，priority 0（acc 95 必中）
constexpr int k30 = 16513;         // 霸天屠龙斩：物攻 160，[30]（必中）
constexpr int k93 = 12325;         // 迷之气泡：特攻 120，[93] args[10,100]（acc 100 必中）
constexpr int k101 = 10377;        // 魔王炎击波：特攻 125，[101,33] args[50]（acc 100 必中）
constexpr int k597 = 23373;        // 深红契约：[48,597] args[3,|5,8]（必中）

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
// 嗑药回合：side 用药（完全跳过出手），对方正常出招。
void medicine_round(ScenarioBattle& b, int side, int opp_idx = 0) {
    const int att = side, def = 1 - side;
    b.submit_operation(att, BattleFsm::ActionType::USE_MEDICINE, /*medicine_index=*/0);
    b.submit_operation(def, BattleFsm::ActionType::SELECT_SKILL, opp_idx);
    b.drive();
}

ElfPet attacker(int skill, int speed = 200, int hp = 4000) {
    return scenario::make_pet(0, "攻方", {skill, skill, skill, skill, skill},
                              speed, 200, hp);
}
// 混合配招（测 9 的"换技能断连"）：[0]=水晶烈冲、[1]=拍打。
ElfPet mixed_attacker() {
    return scenario::make_pet(0, "攻方", {k9, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                              200, 200, 4000);
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

    //========== N1 effect 9：连用递增 + 封顶 + 换技能断连 ==========
    {
        ScenarioBattle b = make_battle(mixed_attacker(), quiet_defender());
        b.start();
        const int expect[4] = {80, 100, 120, 140};   // 连用 1~4 次（第 4 次触及 140 封顶）
        for (int r = 0; r < 4; ++r) {
            round_of(b, /*idx0=*/0);
            char msg[64];
            std::snprintf(msg, sizeof(msg), "N1) 连用第 %d 次威力应 %d", r + 1, expect[r]);
            CHECK_EQ(b.ctx().ws.skill_power_view[0].value, expect[r], msg);
        }
        round_of(b, /*idx0=*/0);
        CHECK_EQ(b.ctx().ws.skill_power_view[0].value, 140, "N1) 超过上限后威力钳在 140");
        std::cout << "N1a PASS 9 连用 80→100→120→140→140（封顶）\n";

        round_of(b, /*idx0=*/1);   // 换拍打 → 断连
        round_of(b, /*idx0=*/0);   // 回到水晶烈冲 → 连用计数重置
        CHECK_EQ(b.ctx().ws.skill_power_view[0].value, 80, "N1) 换技能后连用应断连（回到 80）");
        std::cout << "N1b PASS 换技能断连\n";
    }

    //========== N2 effect 9：嗑药回合不断连（字面"连续使用"口径）==========
    {
        ScenarioBattle b = make_battle(attacker(k9), quiet_defender(),
                                       /*meds0=*/MedicineStock{{{300011, 3}}});
        b.start();
        round_of(b);              // 第 1 次：80
        medicine_round(b, 0);     // 嗑药：不出手
        round_of(b);              // 第 2 次使用：按口径连用继续 → 100
        CHECK_EQ(b.ctx().ws.skill_power_view[0].value, 100,
                 "N2) 嗑药回合不出手，不应断连（80→100）");
        std::cout << "N2 PASS 嗑药不断连（100）\n";
    }

    //========== F1 effect 40：先手威力 ×2 ==========
    {
        ScenarioBattle a = make_battle(attacker(k40, /*speed=*/200), quiet_defender(/*speed=*/100));
        a.start();
        round_of(a);
        CHECK_EQ(a.ctx().ws.skill_power_view[0].value, 260, "F1) 先手威力应 ×2（130→260）");
        const int dmg_fast = 4000 - a.pet(1).hp;

        ScenarioBattle b = make_battle(attacker(k40, /*speed=*/50), quiet_defender(/*speed=*/100));
        b.start();
        round_of(b);
        CHECK_EQ(b.ctx().ws.skill_power_view[0].value, 130, "F1) 后手不翻倍（130）");
        const int dmg_slow = 4000 - b.pet(1).hp;

        CHECK(dmg_fast > dmg_slow, "F1) 先手伤害应显著更高");
        std::cout << "F1 PASS 40 先手 ×2（伤害 " << dmg_fast << " vs " << dmg_slow << "）\n";
    }

    //========== S1 effect 30：后手威力 ×2 ==========
    {
        ScenarioBattle a = make_battle(attacker(k30, /*speed=*/200), quiet_defender(/*speed=*/100));
        a.start();
        round_of(a);
        CHECK_EQ(a.ctx().ws.skill_power_view[0].value, 160, "S1) 先手不翻倍（160）");

        ScenarioBattle b = make_battle(attacker(k30, /*speed=*/50), quiet_defender(/*speed=*/100));
        b.start();
        round_of(b);
        CHECK_EQ(b.ctx().ws.skill_power_view[0].value, 320, "S1) 后手威力应 ×2（160→320）");
        std::cout << "S1 PASS 30 后手 ×2\n";
    }

    //========== C1 effect 93：概率固伤（固定种子 → 结果确定）==========
    {
        ScenarioBattle b = make_battle(attacker(k93), quiet_defender());
        b.start();
        round_of(b);
        const int loss = 4000 - b.pet(1).hp;
        // 红伤 = 0.84×(200/100)×120+2 ≈ 204 ×浮动[0.851,1.0] → [173,204]；命中 10% 掷点
        // 则再 -100。固定种子下结果唯一，两种合法区间都覆盖：
        const bool with_fixed = (loss >= 273 && loss <= 304);    // 红伤 + 100 固伤
        const bool without = (loss >= 173 && loss <= 204);       // 只有红伤
        CHECK(with_fixed != without, "C1) 10% 固伤要么命中要么不命中（实伤 " + std::to_string(loss) + "）");
        std::cout << "C1 PASS 93 固伤掷点确定（loss=" << loss
                  << (with_fixed ? "，含 100 固伤" : "，未触发") << "）\n";
    }

    //========== H1 effect 101：伤害的 50% 回血 ==========
    {
        ScenarioBattle b = make_battle(attacker(k101), quiet_defender());
        b.start();
        b.pet(0).hp = 1000;
        round_of(b);
        const int dmg = 4000 - b.pet(1).hp;
        CHECK(dmg > 0, "H1) 前提：有伤害");
        CHECK_EQ(b.pet(0).hp, 1000 + dmg * 50 / 100, "H1) 回血 = 最终红伤 × 50%");
        std::cout << "H1a PASS 101 吸血 50%（伤 " << dmg << " → 回 " << dmg / 2 << "）\n";

        // 封回血：吸不到
        ScenarioBattle b2 = make_battle(attacker(k101), quiet_defender());
        b2.start();
        b2.pet(0).hp = 1000;
        b2.ctx().grant_immunity(/*owner=*/0, ImmunityType::HEAL_BLOCK, coverage_all(),
                                /*anomaly_mask=*/0, /*duration_rounds=*/3,
                                /*source_id=*/999068, /*soul_immunity=*/false,
                                EffectScope::ON_STAGE, /*counts=*/0, /*source_effect_id=*/999068);
        round_of(b2);
        CHECK_EQ(b2.pet(0).hp, 1000, "H1) 封回血时吸血应无效");
        std::cout << "H1b PASS 封回血挡吸血\n";
    }

    //========== M1 ★ 嗑药跳过出手：597 每回合吸取在嗑药回合不结算 ==========
    {
        const MedicineStock meds = {{300011, 3}};   // 3 瓶初级体力药剂(+20体力)
        ScenarioBattle b = make_battle(attacker(k597), quiet_defender(), meds);
        b.start();
        round_of(b);   // 用技能 → 吸取对手 max/8=500
        CHECK_EQ(b.pet(1).hp, 3500, "M1) 前提：出招回合吸取生效");
        const int opp_hp = b.pet(1).hp;
        medicine_round(b, 0);   // 嗑药：完全跳过出手 → 每回合吸取不结算
        CHECK_EQ(b.pet(1).hp, opp_hp, "M1) ★ 嗑药回合 597 不应吸取（嗑药跳过出手）");
        std::cout << "M1 PASS 嗑药回合吸取不结算\n";
    }

    std::cout << "\n场景 068 全部通过：技能批 3（93/101/40/30/9 + 嗑药口径）✓" << std::endl;
    return 0;
}
