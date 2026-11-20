// 场景 6410 —— J308 状态门异常/回满族（1426 + 554，
//              moves_lib/j_status_gate_anomaly_heal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6410-6419）
//
// 断言口径：
//   - 1426（37187 寒结冰境 [1426,752,1883]，1426=[100,5]）：**对手不处于异常状态**
//     → 100% 令对手**冻伤(5)**。门 = 对手无任何生效异常。
//     观测面 = 对手异常槽（has_active_abnormal_status(1, 5)）——**不拿 hp 当观测面**
//     （异常自身每回合扣血 + 载体 752 回合末扣 1/3 体力都会污染掉血断言）。
//     审计槽：991426100（门 0/1）/991426200（施加结果码；门闭 = -1）。
//   - 554（15309 满月斩 [554] 纯链）：**自身处于能力下降状态**（任一负槽）→ 恢复
//     全部体力。观测 = 当前体力（满血构造 + 单独压当前体力：make_pet 的 hp 同时是
//     最大体力与初始体力，见《场景断言口径坑集》§4）。
//     审计槽：991554100（门 0/1）/991554200（实际恢复量）。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1426 = 37187;   // 寒结冰境 [1426,752,1883] cat2 pow150
constexpr int kCarrier554 = 15309;    // 满月斩 [554] 纯链 cat1 pow140
constexpr int kPaiDa = 10004;

constexpr int kFrostbite = 5;   // 冻伤
constexpr int kBurn = 2;        // 烧伤

constexpr int k1426GateKey = 991426100;    // +owner*2
constexpr int k1426ApplyKey = 991426200;   // +owner*2
constexpr int k554GateKey = 991554100;     // +owner*2
constexpr int k554HealedKey = 991554200;   // +owner*2

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/20000);
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

void set_level(BattleContext& ctx, int side, int slot, int level) {
    ctx.ability_levels[side][slot] = level;
    ctx.ws.view_levels[side][slot] = level;
}

void run_skill(ScenarioBattle& b, int idx) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, idx);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6410.a 1426 门开：对手无异常 → 100% 冻伤 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier1426)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T6410.a 前置：对手开局无冻伤");
        run_skill(b, 0);
        CHECK_EQ(stor(ctx, k1426GateKey + 0), 1, "T6410.a 门开（对手无异常）");
        CHECK_EQ(stor(ctx, k1426ApplyKey + 0),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6410.a 施加结果 = SUCCESS");
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite), "T6410.a 对手挂上冻伤(5)");
    }

    // ══ T6410.b 1426 门闭：对手已有烧伤 → 不再附加冻伤 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier1426)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(static_cast<int>(apply_anomaly(&ctx, 1, kBurn, 2, /*actor=*/0)),
                 static_cast<int>(ApplyAnomalyResult::SUCCESS),
                 "T6410.b 前置：给对手挂上烧伤");
        run_skill(b, 0);
        CHECK_EQ(stor(ctx, k1426GateKey + 0), 0, "T6410.b 门闭（对手已处于异常）");
        CHECK_EQ(stor(ctx, k1426ApplyKey + 0), -1, "T6410.b 门闭 → 未申报");
        CHECK(!ctx.has_active_abnormal_status(1, kFrostbite),
              "T6410.b 对手没有挂上冻伤");
    }

    // ══ T6410.c 554 门开：自身有负槽 + 半血 → 回满 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier554)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2500;          // 压当前体力（最大体力仍 5000）
        set_level(ctx, 0, 2, -1);         // 自身防御 -1（能力下降状态）
        run_skill(b, 0);
        CHECK_EQ(stor(ctx, k554GateKey + 0), 1, "T6410.c 门开（自身处于能力下降）");
        CHECK_EQ(stor(ctx, k554HealedKey + 0), 2500, "T6410.c 实际恢复 2500");
        CHECK_EQ(ctx.getPet(0).hp, 5000,
                 "T6410.c 恢复全部体力（实测 " + std::to_string(ctx.getPet(0).hp) + "）");
    }

    // ══ T6410.d 554 门闭：自身无负槽 + 半血 → 不回满 ══
    {
        ScenarioBattle b(make_party(mk_self(kCarrier554)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 2500;
        run_skill(b, 0);
        CHECK_EQ(stor(ctx, k554GateKey + 0), 0, "T6410.d 门闭（自身无能力下降）");
        CHECK_EQ(stor(ctx, k554HealedKey + 0), 0, "T6410.d 无恢复");
        CHECK_EQ(ctx.getPet(0).hp, 2500,
                 "T6410.d 体力不变（实测 " + std::to_string(ctx.getPet(0).hp) + "）");
    }

    std::cout << "scenario 6410 OK" << std::endl;
    return 0;
}
