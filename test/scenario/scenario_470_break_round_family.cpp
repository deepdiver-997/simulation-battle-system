// 场景 470 —— E2 消回合/消强成功族（453 / 915 / 979 / 1219，moves_lib/e_break_round.cpp）
//（2026-09-29，worktree e-E2）
//
// 家族门槛 = 消除类效果按结果分支：SUCCESS（真的清掉了东西）才执行后续子句。
// 造"对手有回合类效果"：开战后直调 seal_skill 给**对手名下**挂一条回合型封属票
//（330 同款注入——⚠️ 47 免弱票是 IMMUNE 纯查询类，不计入 has_round_type，不可作断料）。
//
// 断言组：
//   T1 453（破败之击 14106，args=[7] 疲惫）：对手有提升 → 提升清零 + 对手疲惫；
//      对照：无提升 → 无疲惫（消强 SUCCESS 门槛）。
//   T2 915（神威灼世斩 30429，args=[2,100,2] 2回合100%烧伤）：
//      消除成功 → 当回合无烧伤（NextRounds 下回合起算）→ R2 攻击命中 → 烧伤落地；
//      对照：对手无窗口（消除失败）→ R2 攻击后无烧伤。
//   T3 979（古灭拳 19951，args=[2,1] 2回合先制+1）：消除成功 → R2 低速翻盘先手
//      （001 同款速度劣势 100 vs 300）；对照：消除失败 → 对手先手。
//   T4 1219（锋刃初明 31042）：消除成功 → R2 挨打 hp 回到挨打前（1:1 全额转化）；
//      对照：消除失败 → R2 挨打 hp 下降。
//
// 随机性口径：T2 用 915 引用里唯一的 100% 切片（30429），确定性断言；453/1219 无概率。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kZhuangJi = 10001;      // 撞击：无效果 priority 0
constexpr int kPobai = 14106;         // 破败之击：物理 [453] args=[7] 疲惫
constexpr int kShenwei = 30429;       // 神威灼世斩：物理 [915,572] 915切片=[2,100,2]（2回合100%烧伤）
constexpr int kGumie = 30184;         // 生死遥控仪：物理 [979,833] 979切片=[2,2]（2回合先制+2）
                                      // ⚠️ 必须用 priority=0 的 979 技能——19951 古灭拳固有
                                      // priority=3 且 ws.preemptive_level 残留跨回合，会污染
                                      // R2 的先手对照（T3 假阳性/假阴性都可能出现）
constexpr int kFengren = 31042;       // 锋刃初明：物理 [1219,1030]（1219 无参；1030 反馈下降）
constexpr int kFatigueId = static_cast<int>(AbnormalStatusId::Fatigue);   // 7
constexpr int kBurnId = static_cast<int>(AbnormalStatusId::Burn);         // 2

ElfPet self_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, /*hp=*/5000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, /*hp=*/100000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int active_anomaly_count(const BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= kOfficialAbnormalStatusMaxId; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

void play_round(ScenarioBattle& b, int self_slot, int foe_slot) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

// 开战后给**对手名下**（source=1）挂一条回合型封属票——己方可断的"回合类效果"
//（330 同款注入；封属性技能不影响全物理技能的测试动作）。
void grant_foe_round_ticket(ScenarioBattle& b, int effect_id) {
    seal_skill(&b.ctx(), /*source=*/1, /*target=*/0, /*effect_id=*/effect_id,
               /*attribute=*/true, /*attack=*/false, /*count=*/0,
               /*duration_rounds=*/3, /*penetrable=*/true, /*source_slot=*/-1,
               EffectScope::ON_STAGE, /*hit_invalid=*/false, /*chance_pct=*/100,
               /*consumed_when_pierced=*/false);
    CHECK(b.ctx().rule_center_.has_round_type(1), "前提：对手持有回合类效果");
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ════════════════════════════════════════════════════════════════
    // T1 453：消强成功 → 对手疲惫；无提升 → 无疲惫
    // ════════════════════════════════════════════════════════════════
    {
        // 主路径：对手有提升
        ScenarioBattle b(make_party(self_pet({kPobai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx.ability_levels[1][i] = 2;   // 开战后写入（on-stage，002 同款）
        }
        play_round(b, 0, 0);
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T1 核心：对手能力提升被消除");
        CHECK(ctx.has_active_abnormal_status(1, kFatigueId),
              "T1 核心：消除成功 → 对手疲惫（{0}=7）");
        std::cout << "  [T1a] 453 消强成功路径 通过\n";
    }
    {
        // 对照：对手无提升 → 门槛失败
        ScenarioBattle b(make_party(self_pet({kPobai, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        play_round(b, 0, 0);
        CHECK(!b.ctx().has_active_abnormal_status(1, kFatigueId),
              "T1 对照：无提升可消 → 不给疲惫");
        std::cout << "  [T1b] 453 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T2 915：消除成功 → 下2回合攻击100%烧伤；消除失败 → 无烧伤
    // ════════════════════════════════════════════════════════════════
    {
        // 主路径：R1 己方神威灼世斩消除对手票 SUCCESS（NextRounds 窗口 R2,R3）
        ScenarioBattle b(make_party(self_pet({kShenwei, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        grant_foe_round_ticket(b, 70471);
        play_round(b, 0, 0);
        CHECK(!ctx.has_active_abnormal_status(1, kBurnId),
              "T2 前置：消除当回合不烧伤（NextRounds 本回合不算）");
        // R2 己方攻击命中 → 烧伤落地（100% 切片，确定性）
        play_round(b, 1, 1);
        CHECK(ctx.has_active_abnormal_status(1, kBurnId),
              "T2 核心：下回合攻击命中 → 对手烧伤（{0}=2，100% 切片）");
        std::cout << "  [T2a] 915 消除成功→窗口烧伤 通过\n";
    }
    {
        // 对照：对手无回合类窗口 → 消除失败 → R2 攻击后无烧伤
        ScenarioBattle b(make_party(self_pet({kShenwei, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        play_round(b, 1, 1);
        CHECK(!ctx.has_active_abnormal_status(1, kBurnId),
              "T2 对照：消除失败（对手无窗口）→ 无烧伤");
        std::cout << "  [T2b] 915 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T3 979：消除成功 → 下回合低速翻盘先手；消除失败 → 对手先手
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kGumie, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        grant_foe_round_ticket(b, 70472);
        play_round(b, 0, 0);   // R1：己方古灭拳消除 SUCCESS（先制窗口 R2,R3）
        play_round(b, 1, 1);   // R2：双方普攻 priority 0 → 己方先制+1 翻盘（速度 100 vs 300）
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T3 核心：消除成功 → 下回合先制+1 → 低速翻盘先手");
        std::cout << "  [T3a] 979 消除成功→翻盘先手 通过\n";
    }
    {
        // 对照：对手无窗口 → 消除失败 → R2 高速对手先手
        ScenarioBattle b(make_party(self_pet({kGumie, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})));
        b.start();
        play_round(b, 0, 0);
        play_round(b, 1, 1);
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T3 对照：消除失败 → 无先制加成 → 高速对手先手");
        std::cout << "  [T3b] 979 门槛对照 通过\n";
    }

    // ════════════════════════════════════════════════════════════════
    // T4 1219：消除成功 → R2 挨打全额回血；消除失败 → 挨打掉血
    // ════════════════════════════════════════════════════════════════
    {
        ScenarioBattle b(make_party(self_pet({kFengren, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/200)));
        b.start();
        grant_foe_round_ticket(b, 70473);
        play_round(b, 0, 0);   // R1：己方锋刃初明消除 SUCCESS（watcher R+1 生效）
        const int hp_before = b.ctx().getPet(0).hp;   // R2 开局
        play_round(b, 1, 1);   // R2：对手撞击打出伤害 d → 己方回复 d
        CHECK_EQ(b.ctx().getPet(0).hp, hp_before,
                 "T4 核心：下回合受到的伤害 1:1 转化为体力（挨打后 hp == 挨打前 hp）");
        std::cout << "  [T4a] 1219 消除成功→伤害转化回复 通过\n";
    }
    {
        // 对照：对手无窗口 → 消除失败 → R2 挨打掉血
        ScenarioBattle b(make_party(self_pet({kFengren, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi})),
                         make_party(foe_pet({kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                                            /*speed=*/300, /*atk=*/200)));
        b.start();
        BattleContext& ctx = b.ctx();
        play_round(b, 0, 0);
        const int hp_before = ctx.getPet(0).hp;
        play_round(b, 1, 1);
        CHECK(ctx.getPet(0).hp < hp_before,
              "T4 对照：消除失败 → 无转化 → 挨打掉血");
        std::cout << "  [T4b] 1219 门槛对照 通过\n";
    }

    std::cout << "scenario_470 PASS (E2 消回合/消强成功族 453/915/979/1219)" << std::endl;
    return 0;
}
