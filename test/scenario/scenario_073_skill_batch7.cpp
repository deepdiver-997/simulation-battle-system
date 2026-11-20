#include <utils/sbs_rand.h>
// 场景 073：技能批 7 —— 下N回合增益 + 对手限制窗口族（2026-09-17）。
//
// 口径来源：effect_info 模板原文（数据库为准）+ 图鉴原文抽样核对：
//   - 481 下{0}回合自身**攻击**先制+{1}（图鉴抽样：黑夜之光/幻象残影/次元刀锋均写"自身攻击先制"；
//         模板带"攻击"字样 → 限定攻击技能，属性技能不吃）
//   - 681 下{0}回合自身**攻击技能**必定致命、必定命中（无尽圣威/智慧之光）
//   - 478 {0}回合内令对手使用的属性技能无效（智慧之光"对手2回合内无法使用属性技能"）
//   - 679 {0}回合内对手无法通过自身技能恢复体力（王·异甲同心"使对手2回合内无法通过技能回血"）
//   - 125 {0}回合内每回合被攻击所受到的伤害不超过{1}（锁伤，管线 CAP 阶段）
//
// 验证点：
//   P1 481：窗口内用**攻击**技能 → 先制+3（低速抢到先手）；用**属性**技能 → 不加速
//   P2 681：窗口内攻击必定暴击（crit_rate=0 也暴）；对手 100% 闪避下仍命中（必中）
//   P3 478：窗口内对手用属性技能 → 被失效；用攻击技能 → 照常
//   P4 679：窗口内对手回血被封；窗口过期后恢复
//   P5 125：受击伤害被锁在 280；未开窗时远超 280
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
constexpr int k481 = 23009;        // 速度之魂：[481] args[1,3] → 下1回合攻击先制+3（属性·必中）
constexpr int k681 = 23969;        // 苍茫之魂：[681] args[2] → 下2回合攻击必暴必中（属性·必中）
constexpr int k478 = 21893;        // 退魔封灵：[478] args[2] → 2回合对手属性技能无效（属性·必中）
constexpr int k679 = 31890;        // 针叶暴雨：[679] args[2] → 2回合对手无法回血（特攻 130）
constexpr int k125 = 21148;        // 钢筋铁骨：[125] args[3,280] → 3回合受击不超过280（属性·必中）

using scenario::ScenarioBattle;

ScenarioBattle make_battle(const ElfPet& p0, const ElfPet& p1) {
    sbs_srand(20260916);
    return ScenarioBattle(scenario::make_party(p0), scenario::make_party(p1));
}

void round_of(ScenarioBattle& b, int idx0 = 0, int idx1 = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, idx1);
    b.drive();
}

constexpr int kSlow = 50;
constexpr int kFast = 300;

ElfPet mine(int skill, int speed = 200) {
    return scenario::make_pet(0, "我方", {skill, kPaiDa, kGaoSu, kGaoSu, kGaoSu},
                              speed, 200, 4000);
}
ElfPet foe(int skill = kGaoSu, int speed = 100) {
    return scenario::make_pet(1, "对手", {skill, kPaiDa, kGaoSu, kGaoSu, kGaoSu},
                              speed, 100, 4000);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    //========== P1 effect 481：下1回合攻击先制+3（攻击限定）==========
    {
        // 第 1 回合（我方速度 50 慢于对手 300）：开窗；第 2 回合用**攻击**技能 → 先制+3 抢到手。
        ScenarioBattle b = make_battle(mine(k481, kSlow), foe(kGaoSu, kFast));
        b.start();
        round_of(b, /*idx0=*/0, /*idx1=*/0);   // 我方：速度之魂（属性）
        CHECK(b.ctx().preemptive_right != PreemptiveRight::SEER_ROBOT_1,
              "P1) 前提：开窗回合我方仍是后手");
        round_of(b, /*idx0=*/1, /*idx1=*/0);   // 我方：拍打（攻击技能）
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "P1) 窗口内用攻击技能 → 先制+3 抢到先手");
        std::cout << "P1a PASS 481 攻击技能先制+3（低速抢到手）\n";

        // 同样开窗后改用**属性**技能 → 不吃先制，仍然后手
        ScenarioBattle b2 = make_battle(mine(k481, kSlow), foe(kGaoSu, kFast));
        b2.start();
        round_of(b2, 0, 0);                    // 开窗
        round_of(b2, /*idx0=*/2, /*idx1=*/0);  // 我方：高速移动（属性技能）
        CHECK(b2.ctx().preemptive_right != PreemptiveRight::SEER_ROBOT_1,
              "P1) 属性技能不吃 481 的先制（仍后手）");
        std::cout << "P1b PASS 481 属性技能不加速\n";

        // 对照组：不开窗 → 攻击技能也抢不到
        ScenarioBattle b3 = make_battle(mine(k481, kSlow), foe(kGaoSu, kFast));
        b3.start();
        round_of(b3, /*idx0=*/1, /*idx1=*/0);  // 直接拍打（无窗口）
        CHECK(b3.ctx().preemptive_right != PreemptiveRight::SEER_ROBOT_1,
              "P1) 对照：无窗口时低速抢不到先手");
        std::cout << "P1c PASS 对照无窗口抢不到\n";
    }

    //========== P2 effect 681：下N回合攻击必暴 + 必中 ==========
    {
        // 攻方技能暴击率 0（harness 默认 → 永不必暴）；对手 100% 闪避（无必中必 miss）。
        ElfPet me = mine(k681);
        ElfPet dodgy = foe();
        dodgy.soulMark = scenario::make_dodge_soul_mark();
        ScenarioBattle b = make_battle(me, dodgy);
        b.start();
        round_of(b, /*idx0=*/0, /*idx1=*/2);   // 开窗（苍茫之魂自身带 must_hit → 能中）
        CHECK(b.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT, "P2) 前提：开窗回合自己命中");
        const int hp_before = b.pet(1).hp;
        round_of(b, /*idx0=*/1, /*idx1=*/2);   // 窗口内拍打
        CHECK(b.ctx().ws.skill_exec_result[0] == SkillExecResult::HIT,
              "P2) ★ 对手 100% 闪避下，681 的必中应让攻击命中");
        CHECK_EQ(b.ctx().crit_happened[0], true, "P2) ★ crit_rate=0 也必定暴击");
        CHECK(b.pet(1).hp < hp_before, "P2) 伤害应落地");
        std::cout << "P2a PASS 681 必中（穿 100% 闪避）+ 必暴\n";

        // 对照组：不开窗 → 同类攻击被闪避（miss）
        ScenarioBattle c = make_battle(mine(k681), dodgy);
        c.start();
        round_of(c, /*idx0=*/1, /*idx1=*/2);
        CHECK(c.ctx().ws.skill_exec_result[0] == SkillExecResult::SKILL_INVALID,
              "P2) 对照：无窗口时被闪避（miss）");
        std::cout << "P2b PASS 对照被闪避\n";
    }

    //========== P3 effect 478：令对手属性技能无效 ==========
    {
        ScenarioBattle b = make_battle(mine(k478, kFast), foe(kGaoSu, kSlow));
        b.start();
        round_of(b, /*idx0=*/0, /*idx1=*/0);   // 我方封属
        round_of(b, /*idx0=*/2, /*idx1=*/0);   // 对手（速度慢）用属性技能 → 应被失效
        CHECK(b.ctx().ws.skill_exec_result[1] == SkillExecResult::SKILL_INVALID,
              "P3) 窗口内对手属性技能应被失效");
        std::cout << "P3a PASS 478 封对手属性技能\n";

        // 攻击技能不受封
        ScenarioBattle b2 = make_battle(mine(k478, kFast), foe(kPaiDa, kSlow));
        b2.start();
        round_of(b2, 0, 0);                    // 封属
        const int hp0 = b2.pet(0).hp;
        round_of(b2, 2, /*idx1=*/1);           // 对手用拍打（攻击技能）
        CHECK(b2.ctx().ws.skill_exec_result[1] == SkillExecResult::HIT,
              "P3) 攻击技能不受 478 影响");
        CHECK(b2.pet(0).hp < hp0, "P3) 攻击技能照常造成伤害");
        std::cout << "P3b PASS 478 不封攻击技能\n";
    }

    //========== P4 effect 679：对手无法回血 ==========
    {
        ScenarioBattle b = make_battle(mine(k679, kFast), foe(kGaoSu, kSlow));
        b.start();
        round_of(b, /*idx0=*/0, /*idx1=*/0);   // 我方封对手回血
        b.pet(1).hp = 1000;
        (void)heal_amount(&b.ctx(), 1, 500);
        CHECK_EQ(b.pet(1).hp, 1000, "P4) 窗口内对手回血应被封");
        std::cout << "P4a PASS 679 封对手回血\n";

        // 对照组：我方改用高速移动（不封）→ 对手回血正常
        ScenarioBattle b2 = make_battle(mine(kGaoSu, kFast), foe(kGaoSu, kSlow));
        b2.start();
        round_of(b2, 0, 0);
        b2.pet(1).hp = 1000;
        (void)heal_amount(&b2.ctx(), 1, 500);
        CHECK_EQ(b2.pet(1).hp, 1500, "P4) 对照：无 679 时回血正常");
        std::cout << "P4b PASS 对照回血正常\n";
    }

    //========== P5 effect 125：受击伤害被锁在 280 ==========
    {
        // 我方先手开锁伤窗（3 回合、上限 280）；对手高攻拍打（裸伤远超 280）。
        ElfPet me = scenario::make_pet(0, "我方", {k125, k125, k125, k125, k125},
                                       kFast, 100, 4000);
        ElfPet hitter = scenario::make_pet(1, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                           kSlow, 2000, 4000);
        ScenarioBattle b = make_battle(me, hitter);
        b.start();
        const int hp0 = b.pet(0).hp;
        round_of(b, /*idx0=*/0, /*idx1=*/0);   // 我方开窗 + 对手同回合攻击
        const int capped = hp0 - b.pet(0).hp;
        CHECK_EQ(capped, 280, "P5) 受击伤害应被锁在 280（裸伤远超）");
        std::cout << "P5a PASS 125 锁伤 280\n";

        // 对照组：不开窗 → 裸伤远超 280
        ScenarioBattle c = make_battle(
            scenario::make_pet(0, "我方", {kGaoSu, kGaoSu, kGaoSu, kGaoSu, kGaoSu},
                               kFast, 100, 4000),
            scenario::make_pet(1, "打手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                               kSlow, 2000, 4000));
        c.start();
        const int hp0c = c.pet(0).hp;
        round_of(c, 0, 0);
        const int raw = hp0c - c.pet(0).hp;
        CHECK(raw > 280, "P5) 对照裸伤应远超 280（实际 " + std::to_string(raw) + "）");
        std::cout << "P5b PASS 对照裸伤 " << raw << " > 280\n";
    }

    std::cout << "\n场景 073 全部通过：技能批 7（481/681/478/679/125）✓" << std::endl;
    return 0;
}
