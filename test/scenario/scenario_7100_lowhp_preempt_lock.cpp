#include <utils/sbs_rand.h>
// 场景 7100 —— K365 低血先制+锁血残血族（790，soul_lib/k365_lowhp_preempt_lock.cpp）
//（2026-10-24，worktree ../agents/k365；号段 7100-7109 → T7100-T7105 六块）
//
// 断言口径：
//   - ①先制：FMR 停泊读 ws.preemptive_level ==2（memset 后写；印主速 100 < 对手
//     300，无先制必后手——结构反证）。
//   - ①害怕：foe 攻击印主后 has_active_abnormal_status(1, Fear)（apply_anomaly
//     100% 通道，1067 烧伤同款）；非低血回合/boss 局 → 无害怕。
//   - ②锁血：印主 hp 压到 300、对手 atk1200（raw∈[347,408] 常识锚）一击致死 →
//     hp == db[1]（1）；R2 FMR pl == db[2]；rider = 对手最大体力/db[3] = 266666
//     精确固伤（拍打红伤 1~2 点叠加 → 掉血带 [266667,266668]）。
//   - ②锁血仅登场首回合（entry_round 锚）：次回合致死 → 真死。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHit = 10004;
constexpr int kMaxHp = 800000;
constexpr int kFear = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kRider = kMaxHp / 3;   // 266666

SoulMark make_mark(const std::vector<int>& args) {
    // DB args 原序：db[0]=①先制 db[1]=②残留血 db[2]=②先制 db[3]=②固伤分母。
    SoulMark sm(790, "K365 魂印", "test", EffectArgs(args));
    sm.monster_id = 3141;
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args) {
    ElfPet p = make_pet(1, "深渊狱神·哈迪斯", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/100, /*atk=*/1, kMaxHp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet(int atk) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit},
                        /*speed=*/300, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

void foe_attack_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

// R2 停泊：印主选攻击技（rider 语义需要），读 FMR 后先制，再放行整回合。
int read_preempt_then_drive(ScenarioBattle& b) {
    b.ctx().debug_step_mode = true;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.ctx().debug_step_mode = false;
    b.step_until(State::BATTLE_ROUND_START);
    b.step();
    b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
    b.step();   // 执行 FMR（memset 后写）
    const int pl = b.ctx().ws.preemptive_level[0];
    b.drive();
    return pl;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7100 ①低血：当回合先制+2 + 受击对手害怕（row480 db[0]=2）═════════════
    {
        sbs_srand(7100);
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 3})), make_party(foe_pet(1)));
        b.start();
        b.ctx().getPet(0).hp = 100000;   // 回合开始前压低（< 对手 800000）
        int pl = -1;
        {
            b.ctx().debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.ctx().debug_step_mode = false;
            b.step_until(State::BATTLE_ROUND_START);
            b.step();
            b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
            b.step();
            pl = b.ctx().ws.preemptive_level[0];
            b.drive();
        }
        CHECK_EQ(pl, 2, "T7100 核心：回合开始低血 → 当回合先制+2");
        CHECK(b.ctx().has_active_abnormal_status(1, kFear),
              "T7100 受击 → 对手害怕");
        std::cout << "  [T7100] 低血先制+2+害怕 通过\n";
    }

    // ══ T7101 ①负门：体力不低于对手 → 无先制无害怕 ═══════════════════════════
    {
        sbs_srand(7101);
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 3})), make_party(foe_pet(1)));
        b.start();   // 双方同血 800000（不低）
        int pl = -1;
        {
            b.ctx().debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.ctx().debug_step_mode = false;
            b.step_until(State::BATTLE_ROUND_START);
            b.step();
            b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
            b.step();
            pl = b.ctx().ws.preemptive_level[0];
            b.drive();
        }
        CHECK_EQ(pl, 0, "T7101 体力不低于对手 → 无先制");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear),
              "T7101 非低血回合 → 不害怕");
        std::cout << "  [T7101] 负门 通过\n";
    }

    // ══ T7102 ②锁血+下回合先制+攻击 rider（row480 db[2]=2 db[3]=3）════════════
    {
        sbs_srand(7102);
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 3})), make_party(foe_pet(1200)));
        b.start();
        b.ctx().getPet(0).hp = 300;   // 致死量：raw [347,408] > 300
        foe_attack_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1, "T7102 核心：登场首回合致命伤残留 1 血");
        b.ctx().getPet(0).hp = kMaxHp;   // 回补血量关①门，隔离②断言
        const int foe0 = b.ctx().getPet(1).hp;
        const int pl = read_preempt_then_drive(b);   // R2：印主攻击
        CHECK_EQ(pl, 2, "T7102 锁血次回合 → 攻击先制+2");
        const int drop = foe0 - b.ctx().getPet(1).hp;
        CHECK(drop >= kRider + 1 && drop <= kRider + 2,
              "T7102 攻击附加对手最大体力1/3固伤（拍打 1~2 叠加；实测 -" +
                  std::to_string(drop) + "）");
        std::cout << "  [T7102] 锁血+先制+2+rider 通过（-"<< drop <<"）\n";
    }

    // ══ T7103 boss：①全链不生效，②照常 ═══════════════════════════════════════
    {
        sbs_srand(7103);
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 3})), make_party(foe_pet(1)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 100000;   // 低血（boss 下①门不开）
        int pl = -1;
        {
            b.ctx().debug_step_mode = true;
            b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.ctx().debug_step_mode = false;
            b.step_until(State::BATTLE_ROUND_START);
            b.step();
            b.step_until(State::BATTLE_FIRST_MOVE_RIGHT);
            b.step();
            pl = b.ctx().ws.preemptive_level[0];
            b.drive();
        }
        CHECK_EQ(pl, 0, "T7103 boss：低血先制不生效");
        CHECK(!b.ctx().has_active_abnormal_status(1, kFear),
              "T7103 boss：受击不害怕（①子句 boss 无效）");
        std::cout << "  [T7103] boss ①不生效 通过\n";
    }
    {
        sbs_srand(7104);
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 3})), make_party(foe_pet(1200)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 300;
        foe_attack_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1, "T7103 boss：锁血（②无 boss 尾注）照常");
        b.ctx().getPet(0).hp = kMaxHp;
        const int foe0 = b.ctx().getPet(1).hp;
        const int pl = read_preempt_then_drive(b);
        CHECK_EQ(pl, 2, "T7103 boss：锁血次回合先制+2 照常");
        const int drop = foe0 - b.ctx().getPet(1).hp;
        CHECK(drop >= kRider + 1 && drop <= kRider + 2,
              "T7103 boss：rider 照常（实测 -" + std::to_string(drop) + "）");
        std::cout << "  [T7103] boss ②照常 通过（-" << drop << "）\n";
    }

    // ══ T7104 进化行 args[2,1,3,3]：②先制 +3 ═════════════════════════════════
    {
        sbs_srand(7105);
        ScenarioBattle b(make_party(mark_pet({2, 1, 3, 3})), make_party(foe_pet(1200)));
        b.start();
        b.ctx().getPet(0).hp = 300;
        foe_attack_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 1, "T7104 前置：锁血触发");
        b.ctx().getPet(0).hp = kMaxHp;
        const int pl = read_preempt_then_drive(b);
        CHECK_EQ(pl, 3, "T7104 进化行：锁血次回合攻击先制+3");
        std::cout << "  [T7104] 进化行先制+3 通过\n";
    }

    // ══ T7105 锁血仅登场首回合：次回合致死 → 真死 ═════════════════════════════
    {
        sbs_srand(7106);
        ScenarioBattle b(make_party(mark_pet({2, 1, 2, 3})), make_party(foe_pet(1200)));
        b.start();   // R1 满血无致死
        foe_attack_round(b);
        CHECK(b.ctx().getPet(0).hp > 0, "T7105 前置：R1 满血不致死");
        b.ctx().getPet(0).hp = 300;   // R2 才压到致死区——已非登场首回合
        foe_attack_round(b);
        CHECK_EQ(b.ctx().getPet(0).hp, 0,
                 "T7105 核心：非登场首回合致命伤不锁血（entry_round 锚）");
        std::cout << "  [T7105] 锁血仅登场首回合 通过\n";
    }

    std::cout << "scenario_7100: all green" << std::endl;
    return 0;
}
