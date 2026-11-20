// 场景 6340 —— J301 击败先制族（1587，moves_lib/j_kill_preempt.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6340-6349）
//
// 断言口径：
//   - 1587（35724 斩魂音刃，纯链 [1587]|[1]）：当回合击败对手 → 下回合**攻击技能**
//     先制 +{0}（MOVE_RIGHT 桶）。属性技不加分（481/J4 判据）。
//   - 击败构造：对手 6 只均 hp=1；己方 atk=300 一击必杀。非击败档 hp=20000/atk=1。
//   - 观测面 = ws.preemptive_level[0]（MOVE_RIGHT 写入后保留到回合结束；本场景 drive
//     完即读）。审计槽 991587100（击败门）/991587200（实际写入值）佐证前提。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1587 = 35724;   // 斩魂音刃 [1587]|[1] 物140（纯链）
constexpr int kPaiDa = 10004;
constexpr int kDengYan = 20004;

constexpr int kGateKey = 991587100;   // +self
constexpr int kBonusKey = 991587200;  // +self

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int atk) {
    ElfPet p = make_pet(1, "己方", {kCarrier1587, kPaiDa, kDengYan, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp, int atk) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, atk, hp);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

void run_round(ScenarioBattle& b, int skill_idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, skill_idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6340.a 击败 → 下回合攻击先制 +1 ══
    {
        ScenarioBattle b(make_party(mk_self(/*atk=*/300)), make_party(mk_foe(/*hp=*/1, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0);   // R1 载体 KO
        CHECK_EQ(stor(ctx, kGateKey + 0), 1, "T6340.a 击败门开");
        CHECK_EQ(static_cast<int>(ctx.currentState),
                 static_cast<int>(State::CHOOSE_AFTER_DEATH), "T6340.a 前置：停在死切");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        run_round(b, 1);   // R2 攻击技
        CHECK_EQ(stor(ctx, kBonusKey + 0), 1, "T6340.a 攻击技 → 写入 +1");
        CHECK_EQ(ctx.ws.preemptive_level[0], 1,
                 "T6340.a 下回合攻击先制 +1（实测 " +
                     std::to_string(ctx.ws.preemptive_level[0]) + "）");
    }

    // ══ T6340.b 属性技不加分（"攻击先制"限定）══
    {
        ScenarioBattle b(make_party(mk_self(/*atk=*/300)), make_party(mk_foe(/*hp=*/1, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0);   // R1 载体 KO
        CHECK_EQ(stor(ctx, kGateKey + 0), 1, "T6340.b 击败门开");
        b.submit_operation(1, BattleFsm::ActionType::CHOOSE_PET, 1);
        b.drive();
        run_round(b, 2);   // R2 属性技（瞪眼）
        CHECK_EQ(stor(ctx, kBonusKey + 0), 0, "T6340.b 属性技 → 不写入 bonus");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                 "T6340.b 属性技不加先制（实测 " +
                     std::to_string(ctx.ws.preemptive_level[0]) + "）");
    }

    // ══ T6340.c 未击败 → 门闭，下回合不加先制 ══
    {
        ScenarioBattle b(make_party(mk_self(/*atk=*/1)), make_party(mk_foe(/*hp=*/20000, 1)));
        b.start();
        BattleContext& ctx = b.ctx();
        run_round(b, 0);   // R1 载体，打不死
        CHECK_EQ(stor(ctx, kGateKey + 0), 0, "T6340.c 未击败 → 门闭");
        run_round(b, 1);   // R2 攻击技
        CHECK_EQ(stor(ctx, kBonusKey + 0), 0, "T6340.c 未武装 → 无 bonus");
        CHECK_EQ(ctx.ws.preemptive_level[0], 0,
                 "T6340.c 不加先制（实测 " +
                     std::to_string(ctx.ws.preemptive_level[0]) + "）");
    }

    std::cout << "scenario 6340 OK" << std::endl;
    return 0;
}
