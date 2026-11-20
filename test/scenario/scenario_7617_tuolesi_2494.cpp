#include <utils/sbs_rand.h>
// 场景 7617 —— K404 effect 2494 托勒斯（4887，row2074 本体
// args[100,15,200,5,500,1] kind[5,14] / row2103 伙伴「黄金圣徒」args[80,15,200,5,500,1]）
// 「自身先制等级不高于0；自身每次受到的攻击伤害不超过自身最大体力的100%，
//   对手每有1级先制等级该比例降低15%，自身受到攻击后恢复200×（200%-该比例）；
//   自身每次造成的攻击伤害不低于对手最大体力的40%，对手每有1级先制等级该比例
//   提高5%，自身造成攻击伤害后附加500*该比例的固定伤害；自身被击败时令对手
//   1回合内先制等级归0（boss无效）」 —— soul_lib/k_high_dmg_convert.cpp
//
// 断言口径（全精确断言——帽/地板把随机浮动削平）：
//   - 载体 = 光闪击 10630（priority 3 纯链攻击技，无 side_effect）：对手 pl=3 →
//     帽比例 100-15×3=55% → 帽值 54999 精确；恢复 200×(200-55)/100=290 精确；
//     地板比例 40+5×3=55% → 地板 54999 + 附加 500×55%=275 → 对手损失 55274 精确；
//   - 门槛对照 = 印主用光闪击（pl=3>0）→ 帽/恢复/地板/附加全哑
//     （损失带 [76125,89250] 原生无钳 + last_heal_amount=0 + 对手损失纯红伤带）；
//   - 行≠宏差位 = 伙伴行 80%：帽比例 80-45=35% → 帽 34999 / 恢复 330 精确；
//   - 死亡归零 = R1 击倒置戳 → R2 priority_nullified 对手 → 先制比较翻转
//     （速度劣势方凭 nullify 抢先手，行为证）+ 戳消费；接班者非 2494 仍生效
//     （ROSTER scope 恒注册，宿主死后消费不断链）；
//   - boss 门 = 帽/恢复/地板/附加/归零全哑（全节点 boss_invalid）；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
#include <array>
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

constexpr int kHit = 10004;       // 拍打（priority 0）
constexpr int kFlash = 10630;     // 光闪击（priority 3，物理，威力 70，side_effect [] 纯链）
constexpr int kMaxHp = 99999;

// 审计/账本键（与 k_high_dmg_convert.cpp 一致；owner=0 段）。
constexpr int kBase = 1024940000;
constexpr int kZeroStamp = 0;

const std::vector<int> kRowBase{100, 15, 200, 5, 500, 1};     // row2074 本体
const std::vector<int> kRowPartner{80, 15, 200, 5, 500, 1};   // row2103 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(2494, "K404 托勒斯", "test", EffectArgs(args));
    sm.monster_id = 4887;
    sm.kind_tags = {5, 14};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 100, int atk = 300,
                int hp = kMaxHp, bool prio_skill = false) {
    const std::array<int, 5> skills =
        prio_skill ? std::array<int, 5>{kFlash, kHit, kHit, kHit, kHit}
                   : std::array<int, 5>{kHit, kHit, kHit, kHit, kHit};
    ElfPet p = make_pet(1, "托勒斯", skills, speed, atk, hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed = 300, int atk = 300, bool prio_skill = false) {
    const std::array<int, 5> skills =
        prio_skill ? std::array<int, 5>{kFlash, kHit, kHit, kHit, kHit}
                   : std::array<int, 5>{kHit, kHit, kHit, kHit, kHit};
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

int read_key(ScenarioBattle& b, int key) {
    const auto it = b.ctx().plugin_storage.find(key);
    if (it == b.ctx().plugin_storage.end()) {
        return -1;
    }
    const auto* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T7617a 门槛内：帽 54999 + 恢复 290 + 地板 54999 + 附加 275（全精确）════
    {
        // 对手光闪击（pl3）atk150000 → raw [76125,89250] > 帽 54999（裕量 >2万）。
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/150000,
                                              /*prio_skill=*/true)));
        b.start();
        sbs_srand(7622);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 印主 pl=0
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手 pl=3
        b.drive();
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T7617a 前置：对手先制等级 3 抢先手");
        CHECK_EQ(b.ctx().getPet(0).hp, 99999 - 54999 + 290,
                 "T7617a 帽 100-15×3=55%→54999 精确 + 恢复 200×(200-55)%=290 精确");
        CHECK_EQ(kMaxHp - b.ctx().getPet(1).hp, 54999 + 275,
                 "T7617a 地板 40+5×3=55%→54999 + 附加 500×55%=275 → 损失 55274 精确");
        std::cout << "  [T7617a] 帽+恢复+地板+附加 全精确 通过\n";
    }

    // ══ T7617b 门槛对照：自身 pl=3>0 → 帽/恢复/地板/附加全哑 ═══════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/300,
                                             kMaxHp, /*prio_skill=*/true)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/150000,
                                              /*prio_skill=*/true)));
        b.start();
        sbs_srand(7623);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // 印主 pl=3
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   // 对手 pl=3
        b.drive();
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T7617b 前置：印主速度 300 先手");
        const int loss = kMaxHp - b.ctx().getPet(0).hp;
        CHECK(loss >= 76125 && loss <= 89250,
              "T7617b 门槛外：损失=原生带 [76125,89250]（无 54999 帽钳）");
        CHECK_EQ(b.ctx().ws.last_heal_amount[0], 0, "T7617b 门槛外：无受攻恢复");
        const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(foe_loss >= 152 && foe_loss <= 178,
              "T7617b 门槛外：对手损失=纯红伤带 [152,178]（无 54999 地板/275 附加）");
        std::cout << "  [T7617b] 门槛对照 通过\n";
    }

    // ══ T7617c 行≠宏差位：伙伴行 80% → 帽 34999 / 恢复 330（地板侧同 55274）═══
    {
        ScenarioBattle b(make_party(mark_pet(kRowPartner, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/150000,
                                              /*prio_skill=*/true)));
        b.start();
        sbs_srand(7624);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(b.ctx().getPet(0).hp, 99999 - 34999 + 330,
                 "T7617c 伙伴行帽 80-45=35%→34999 精确 + 恢复 200×(200-35)%=330 精确");
        CHECK_EQ(kMaxHp - b.ctx().getPet(1).hp, 54999 + 275,
                 "T7617c 地板侧两行同值：损失 55274 精确");
        std::cout << "  [T7617c] 行≠宏差位 通过\n";
    }

    // ══ T7617d 死亡归零：R1 击倒置戳 → R2 对手先制归零翻转先手权（ROSTER 跨死消费）
    {
        // 对手速度 50（劣势）但 pl=3 抢先手；托勒斯 max=100（帽=55%×100=55）、
        // 现血 50 < 帽 → 致死可达（帽全护则会"锁血不死"，max=1 时帽=0 反成无敌）。
        const ElfPet holder = mark_pet(kRowBase, /*speed=*/100, /*atk=*/300, /*hp=*/100);
        const ElfPet ally = plain_pet(/*speed=*/100, /*atk=*/300);
        std::array<ElfPet, 6> own{holder, ally, ally, ally, ally, ally};
        ScenarioBattle b(own, make_party(plain_pet(/*speed=*/50, /*atk=*/150000,
                                                   /*prio_skill=*/true)));
        b.start();
        b.ctx().getPet(0).hp = 50;
        sbs_srand(7625);
        b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();   // R1：对手 pl3 抢先手拍死 hp=1 托勒斯 → 死亡置戳
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_2,
              "T7617d R1 前置：对手 pl=3 抢先手（速度 50<100）");
        CHECK(b.ctx().getPet(0).hp <= 0, "T7617d 前置：托勒斯被击倒");
        CHECK_EQ(read_key(b, kBase + 0 * 16 + kZeroStamp), 1,
                 "T7617d 死亡置次回合戳=roundCount+1（回合 N 时 rc=N-1）");
        b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // 接班（队友，非 2494）
        // R2：对手再选光闪击（pl3）→ nullify 归零 → 速度 100 vs 50 → 接班者抢先。
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK(b.ctx().preemptive_right == PreemptiveRight::SEER_ROBOT_1,
              "T7617d R2 对手先制归零 → 速度反转（无归零应为 SEER_ROBOT_2）");
        CHECK_EQ(read_key(b, kBase + 0 * 16 + kZeroStamp), -1, "T7617d 戳已消费（1回合限）");
        std::cout << "  [T7617d] 死亡归零（ROSTER 跨死消费）通过\n";
    }

    // ══ T7617e boss 门：帽/恢复/地板/附加全哑（全节点 boss_invalid）════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300)),
                         make_party(plain_pet(/*speed=*/100, /*atk=*/150000,
                                              /*prio_skill=*/true)));
        b.ctx().is_boss_challenge = true;
        b.start();
        sbs_srand(7626);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        const int loss = kMaxHp - b.ctx().getPet(0).hp;
        CHECK(loss >= 76125 && loss <= 89250,
              "T7617e boss：损失=原生带（无 54999 帽钳）");
        CHECK_EQ(b.ctx().ws.last_heal_amount[0], 0, "T7617e boss：无受攻恢复");
        const int foe_loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(foe_loss >= 87 && foe_loss <= 102,
              "T7617e boss：对手损失=纯红伤带（无地板/附加）");
        std::cout << "  [T7617e] boss 门 通过\n";
    }

    std::cout << "scenario_7617: all green" << std::endl;
    return 0;
}
