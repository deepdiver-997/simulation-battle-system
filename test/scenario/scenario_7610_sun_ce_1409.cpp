#include <utils/sbs_rand.h>
// 场景 7610 —— K404 effect 1409 小霸王·孙策（3823，row1098 本体
// args[2,1,1,300,300,1,100,1] kind[1,9,14] / row1307 伙伴「天作之合」
// args[2,1,1,300,350,1,120,1]）
// 「战斗阶段结束时，若自身存活且体力低于最大体力的1/2则解除自身异常状态，
//   恢复自身全部体力并造成等量固伤，同时本场战斗结束前自身所有技能先制+1
//   （每场战斗仅触发1次）；|自身造成的攻击伤害高于300则{吸取}对手300点{体力}，
//   自身造成的伤害低于300则下回合攻击技能先制+1且造成的伤害提升100%（boss无效）」
//  —— soul_lib/k_high_dmg_convert.cpp
//
// 断言口径：
//   - clause1 = R 末低体触发：回满 99999 精确 + 对手 -96999（等量固伤=实际恢复量）
//     精确 + 疲惫(7)解除 + 永久账置位；次回合 FMR 先制字段=1（每回合 memset 重写）；
//   - 每场1次 = 同局二次低体不再触发（血不回、异常不解除）；
//   - >300 支 = 吸取 300 精确回血 + 对手损失带 [648,708]（红伤[348,408]+FIXED 300）、
//     不置 pending；<300 支 = pending 戳精确 + 次回合先制字段=1/增伤字段=100 +
//     攻击 ×2 带 [174,204]（ws 每回合 memset → 非累计）；
//   - 行≠宏差位 = 伙伴行吸取 350 / 增伤 120（带与字段双证）；
//   - boss 门 = clause1 全哑 + 吸取/回血/pending 全哑（全节点 boss_invalid）；
//   - 全部 submit_operation + drive() 有限回合，绝不 while(!FINISHED)。
#include <algorithm>
#include <any>
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

constexpr int kHit = 10004;     // 拍打（物理攻击，priority 0 载体，raw [87,102]@atk300）
constexpr int kFatigue = 7;     // 疲惫（无 1/8 maxHP tick 污染，直施）
constexpr int kMaxHp = 99999;

// 审计/账本键（与 k_high_dmg_convert.cpp 一致；owner=0 段）。
constexpr int kBase = 1014090000;
constexpr int kOnceUsed = 0;
constexpr int kPlLedger = 1;
constexpr int kLowStamp = 2;

const std::vector<int> kRowBase{2, 1, 1, 300, 300, 1, 100, 1};     // row1098 本体
const std::vector<int> kRowPartner{2, 1, 1, 300, 350, 1, 120, 1};  // row1307 伙伴

SoulMark make_mark(const std::vector<int>& args) {
    SoulMark sm(1409, "K404 小霸王·孙策", "test", EffectArgs(args));
    sm.monster_id = 3823;
    sm.kind_tags = {1, 9, 14};
    return sm;
}

ElfPet mark_pet(const std::vector<int>& args, int speed = 100, int atk = 300,
                int hp = kMaxHp) {
    ElfPet p = make_pet(1, "小霸王·孙策", {kHit, kHit, kHit, kHit, kHit},
                        speed, atk, hp, make_mark(args));
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet(int speed = 300, int atk = 300) {
    ElfPet p = make_pet(2, "对手", {kHit, kHit, kHit, kHit, kHit}, speed, atk, kMaxHp);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

void idle_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
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

    // ══ T7610a clause1：战阶末低体 → 解异常+回满+等量固伤+永久先制账；每场1次 ══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/100)),
                         make_party(plain_pet(/*speed=*/300)));
        b.start();
        b.ctx().getPet(0).hp = 3000;
        REQUIRE(apply_anomaly(&b.ctx(), 0, kFatigue, 3, 1) == ApplyAnomalyResult::SUCCESS);
        CHECK(b.ctx().has_active_abnormal_status(0, kFatigue), "T7610a 前置：疲惫已挂");
        sbs_srand(7610);
        idle_round(b);   // R1：双方 NONE，ROUND_END 低体触发
        CHECK_EQ(b.ctx().getPet(0).hp, kMaxHp, "T7610a 恢复自身全部体力（99999 精确）");
        CHECK_EQ(b.ctx().getPet(1).hp, 3000,
                 "T7610a 等量固伤=实际恢复量 96999 → 对手 99999-96999=3000 精确");
        CHECK(!b.ctx().has_active_abnormal_status(0, kFatigue), "T7610a 解除自身异常状态");
        CHECK_EQ(read_key(b, kBase + kPlLedger), 1, "T7610a 本场先制+1 永久账置位");
        idle_round(b);   // R2：FMR 消费永久账
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7610a 本场结束前所有技能先制+1");

        // 每场战斗仅触发1次：再压低血+再挂异常 → 不再触发。
        b.ctx().getPet(0).hp = 3000;
        REQUIRE(apply_anomaly(&b.ctx(), 0, kFatigue, 3, 1) == ApplyAnomalyResult::SUCCESS);
        idle_round(b);   // R3
        CHECK_EQ(b.ctx().getPet(0).hp, 3000, "T7610a 每场1次：体力不再回满");
        CHECK(b.ctx().has_active_abnormal_status(0, kFatigue), "T7610a 每场1次：异常不再解除");
        std::cout << "  [T7610a] clause1+每场1次 通过\n";
    }

    // ══ T7610c >300 支：吸取 300 点体力（双段：FIXED 300 + 等量回血），不置 pending ═
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/1200)),
                         make_party(plain_pet(/*speed=*/100)));
        b.start();
        b.ctx().getPet(0).hp = 50000;
        sbs_srand(7611);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R1：印主先手拍 raw [348,408] > 300 → 吸取 300
        const int loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(loss >= 648 && loss <= 708,
              "T7610c 对手损失=红伤[348,408]+吸取FIXED 300 → [648,708]");
        CHECK_EQ(b.ctx().getPet(0).hp, 50300, "T7610c 吸取等量回血 +300 精确");
        CHECK_EQ(read_key(b, kBase + kLowStamp), -1, "T7610c 高于阈值不置 pending 戳");
        idle_round(b);   // R2：无 pending 消费
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 0.0f, "T7610c 无增伤残留");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7610c 无先制残留");
        std::cout << "  [T7610c] >300 吸取支 通过\n";
    }

    // ══ T7610d <300 支：pending 戳 → 下回合先制+1 & 增伤 100%（ws memset 非累计）══
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/300)),
                         make_party(plain_pet(/*speed=*/100)));
        b.start();
        sbs_srand(7612);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R1：raw [87,102] < 300 → 置戳=2
        const int loss1 = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(loss1 >= 87 && loss1 <= 102, "T7610d R1 红伤带 [87,102]（无增伤）");
        CHECK_EQ(read_key(b, kBase + kLowStamp), 1,
                 "T7610d pending 戳=roundCount+1（回合 N 时 rc=N-1）");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 0, "T7610d 当回合无先制（下回合才生效）");
        int foe0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R2：消费戳 → pl=1/amp=100 → ×2 带 [174,204]
        const int loss2 = foe0 - b.ctx().getPet(1).hp;
        CHECK(loss2 >= 174 && loss2 <= 204, "T7610d R2 攻击 ×2 派生带 [174,204]");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7610d R2 先制字段=1");
        CHECK_EQ(b.ctx().ws.damage_add_pct[0], 100.0f, "T7610d R2 增伤字段=100");
        CHECK_EQ(read_key(b, kBase + kLowStamp), 2,
                 "T7610d R2 攻击仍<300 → 再置 R3 戳（持续低于阈值持续授予）");
        foe0 = b.ctx().getPet(1).hp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R3：消费 R2 戳 → 再 ×2；ws memset → 先制仍 1（非累计）
        const int loss3 = foe0 - b.ctx().getPet(1).hp;
        CHECK(loss3 >= 174 && loss3 <= 204, "T7610d R3 攻击仍 ×2（戳逐回合续授）");
        CHECK_EQ(b.ctx().ws.preemptive_level[0], 1, "T7610d R3 先制字段=1（memset 重写非累计）");
        std::cout << "  [T7610d] <300 pending 支 通过\n";
    }

    // ══ T7610e 行≠宏差位：伙伴行吸取 350 / 增伤 120 ═══════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet(kRowPartner, /*speed=*/300, /*atk=*/1200)),
                         make_party(plain_pet(/*speed=*/100)));
        b.start();
        b.ctx().getPet(0).hp = 50000;
        sbs_srand(7613);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        const int loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(loss >= 698 && loss <= 758,
              "T7610e 伙伴行吸取 350：对手损失带 [698,758]");
        CHECK_EQ(b.ctx().getPet(0).hp, 50350, "T7610e 吸取等量回血 +350 精确（300/350 差位）");
        b.ctx().getPet(1).hp = kMaxHp;
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R2：raw [348,408] 仍 >300 → 再吸取；无 pending
        CHECK_EQ(read_key(b, kBase + kLowStamp), -1, "T7610e 高于阈值无戳");
        // 增伤 120 档：换低攻局观测 pending 字段。
        ScenarioBattle b2(make_party(mark_pet(kRowPartner, /*speed=*/300, /*atk=*/300)),
                          make_party(plain_pet(/*speed=*/100)));
        b2.start();
        sbs_srand(7614);
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b2.drive();   // R1 <300 → 戳
        b2.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b2.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b2.drive();   // R2 消费
        CHECK_EQ(b2.ctx().ws.damage_add_pct[0], 120.0f, "T7610e 增伤字段=120（100/120 差位）");
        std::cout << "  [T7610e] 行≠宏差位 通过\n";
    }

    // ══ T7610f boss 门：clause1/吸取/回血/pending 全哑（全节点 boss_invalid）════
    {
        ScenarioBattle b(make_party(mark_pet(kRowBase, /*speed=*/300, /*atk=*/1200)),
                         make_party(plain_pet(/*speed=*/100)));
        b.ctx().is_boss_challenge = true;
        b.start();
        b.ctx().getPet(0).hp = 3000;
        sbs_srand(7615);
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();   // R1：>300 不吸取（先攻击——疲惫会减半攻击伤害，后挂异常）
        const int loss = kMaxHp - b.ctx().getPet(1).hp;
        CHECK(loss >= 348 && loss <= 408, "T7610f boss：对手损失=纯红伤带 [348,408]（无吸取）");
        CHECK_EQ(b.ctx().getPet(0).hp, 3000, "T7610f boss：无吸取回血");
        REQUIRE(apply_anomaly(&b.ctx(), 0, kFatigue, 3, 1) == ApplyAnomalyResult::SUCCESS);
        idle_round(b);   // R2：clause1 不触发
        CHECK_EQ(b.ctx().getPet(0).hp, 3000, "T7610f boss：低体不回满");
        CHECK(b.ctx().has_active_abnormal_status(0, kFatigue), "T7610f boss：异常不解除");
        CHECK_EQ(read_key(b, kBase + kPlLedger), -1, "T7610f boss：永久账未置位");
        std::cout << "  [T7610f] boss 门 通过\n";
    }

    std::cout << "scenario_7610: all green" << std::endl;
    return 0;
}
