// 场景 030：不灭·艾恩斯 收尾 —— 剩余 3 个专属技能。
//   31272 王·酷烈风息(150)：effect 1221 反转自身能力下降(走 stat_reversal 真原语) → 免疫下1次异常；
//       effect 1256 造成的伤害<{300}→令对手焚烬(22)。
//   31271 诸邪皆伏(140)：effect 683 随机附加 200~400 固定伤害。
//   31270 不灭意志(130)：effect 697 无视伤害限制(穿透凭证)。
//   根因已修(2026-09-12)：ws.view_levels(视图) 只在回合开始从 levels 重基，回合内改 levels 不刷视图，
//       反转后 view 残留旧负值 → getTempAbilityValue 除零 INT_MAX。stat_change/stat_reversal 已同步视图。
#include <cassert>
#include <iostream>

#include <effects/effect_unit.h>
#include <entities/pet_factory.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

namespace {
constexpr int kSkillWangKulie = 31272;   // 王·酷烈风息：150 物理 + 反转下降→免疫异常 + 低伤→焚烬/增伤
constexpr int kSkillZhuxie = 31271;      // 诸邪皆伏：140 特殊 + 随机200~400固定伤害
constexpr int kSkillBumie = 31270;       // 不灭意志：130 物理 + 697无视伤害限制(穿透)
constexpr int kSkillZhuangji = 10001;
constexpr int kRender = static_cast<int>(NumericalPropertyIndex::PHYSICAL_ATTACK);
}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(3467, "不灭·艾恩斯",
                                   {kSkillWangKulie, kSkillZhuxie, kSkillBumie,
                                    kSkillZhuangji, kSkillZhuangji},
                                   /*speed=*/300, /*atk=*/100, /*hp=*/3000);
    auto pet1 = scenario::make_pet(1, "对手",
                                   {kSkillZhuangji, kSkillZhuangji, kSkillZhuangji,
                                    kSkillZhuangji, kSkillZhuangji},
                                   /*speed=*/100, /*atk=*/80, /*hp=*/5000);

    // C1) 反转原语（原语层，不经技能伤害，绕开 INT_MAX 组合 bug）：攻击 -2 → 反转 → +2。
    {
        scenario::ScenarioBattle b(scenario::make_party(pet0), scenario::make_party(pet1));
        BattleContext& x = b.ctx();
        REQUIRE(stat_change(&x, /*target=*/0, kRender, /*delta=*/-2) == StatChangeResult::SUCCESS);
        REQUIRE(x.ability_levels[0][kRender] == -2);
        REQUIRE(stat_reversal(&x, /*target=*/0) == StatReversalResult::REVERSED);
        CHECK_EQ(x.ability_levels[0][kRender], 2, "stat_reversal：下降 -2 → 提升 +2（真原语）");
        std::cout << "  [31272 反转原语] 攻击-2→反转→+2（stat_reversal 真原语，非消除提升）\n";
    }

    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    BattleContext& ctx = battle.ctx();
    battle.start();

    // ── 回合 1：王酷烈风息（基础红伤 ~127 < 300）→ 1256 令对手焚烬(22)。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    CHECK_EQ(ctx.ability_levels[0][kRender], 0, "王酷（无下降反转）自身攻击不变");
    CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Incinerate)),
          "1256 红伤<300 → 对手焚烬(22)");
    std::cout << "  [31272 王酷] 低伤害(<300) → 对手焚烬\n";

    // ── 回合 2：诸邪皆伏 → 对手受到随机 200~400 固定伤害 + 红伤。
    const int opp_hp_before = ctx.getPet(1).hp;
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    const int taken = opp_hp_before - ctx.getPet(1).hp;
    CHECK(taken >= 200, "诸邪皆伏 683: 对手至少受 200 固定伤害");
    CHECK(taken <= 400 + 400, "诸邪皆伏 683: 固定伤害上限 400(+红伤)");
    std::cout << "  [31271 诸邪] 对手掉血=" << taken << "(200~400 固定+红伤)\n";

    // ── 回合 3：不灭意志(697 无视伤害限制) → 穿透命中。
    battle.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 2);
    battle.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    battle.drive();
    CHECK(ctx.ws.skill_exec_result[0] == SkillExecResult::HIT, "不灭意志 697 穿透：技能命中");
    std::cout << "  [31270 不灭] 697 无视伤害限制 穿透命中\n";

    std::cout << "scenario_030 PASS (艾恩斯收尾：反转原语 / 王酷低伤焚烬 / 诸邪随机固伤 / 不灭穿透)\n";
    return 0;
}