// 场景 5990 —— J268 双方护盾总量回血族（1497，moves_lib/j_shield_sum_heal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5990-5999 → T5990.a-d 四块）
//
// 断言口径：
//   - 纯链载体 尘沙气旋 35634 [1497]（物130，零参）。
//   - n = 自身 shield_bank_.total() + 对手 total()；恢复走 heal_amount（半血构造可观测）。
//   - T5990.c 的对手盾 2500 > 本次红伤（≈1300）→ 恢复 2500 证明读的是**攻击消耗前**的盾。
//   - 满血封顶块：名义 3000、实际只落地 99（审计 nominal/healed 双槽）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kChenSha = 35634;   // 尘沙气旋：物 [1497]
constexpr int kPaiDa   = 10004;   // 拍打：filler
constexpr int kMaxHp   = 99999;

constexpr int kAuditNominal = 991497200;   // +owner
constexpr int kAuditHealed  = 991497300;   // +owner

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kChenSha, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

void add_shield(BattleContext& ctx, int side, int amount) {
    ctx.getPet(side).shield_bank_.add(/*priority=*/0, amount, /*source_id=*/1497,
                                      /*duration=*/0);
}

void self_skill(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5990.a 双方盾 1000+2000 → 恢复 3000 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        add_shield(ctx, 0, 1000);
        add_shield(ctx, 1, 2000);
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditNominal + 0, -1), 3000,
                 "T5990.a 1497：名义 n=双方盾总和 3000");
        CHECK_EQ(storage_int(ctx, kAuditHealed + 0, -1), 3000,
                 "T5990.a 1497：实际恢复 3000");
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 3000, "T5990.a 1497：体力 1000→4000");
        std::cout << "  [T5990.a] 1497 双方盾 通过\n";
    }

    // ══ T5990.b 无盾 → n=0，不恢复 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditNominal + 0, -1), 0, "T5990.b 1497：无盾 n=0");
        CHECK_EQ(storage_int(ctx, kAuditHealed + 0, -1), 0, "T5990.b 1497：不恢复");
        CHECK_EQ(ctx.getPet(0).hp, 1000, "T5990.b 1497：体力不变");
        std::cout << "  [T5990.b] 1497 无盾 通过\n";
    }

    // ══ T5990.c 仅对手盾 2500（>本次红伤）→ 恢复攻击消耗前的 2500 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 1000;
        add_shield(ctx, 1, 2500);
        self_skill(b);
        CHECK(ctx.ws.raw_attack_damage[0] > 0, "T5990.c 前置：红伤落地");
        CHECK_EQ(storage_int(ctx, kAuditNominal + 0, -1), 2500,
                 "T5990.c 1497：读攻击前对手盾 2500");
        CHECK_EQ(ctx.getPet(0).hp, 1000 + 2500, "T5990.c 1497：体力 1000→3500");
        std::cout << "  [T5990.c] 1497 仅对手盾 通过\n";
    }

    // ══ T5990.d 满血封顶：名义 3000、实际 99 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.getPet(0).hp = 99900;
        add_shield(ctx, 0, 1000);
        add_shield(ctx, 1, 2000);
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditNominal + 0, -1), 3000,
                 "T5990.d 1497：名义 n=3000");
        CHECK_EQ(storage_int(ctx, kAuditHealed + 0, -1), 99,
                 "T5990.d 1497：实际恢复 99（封顶审计）");
        CHECK_EQ(ctx.getPet(0).hp, 99999, "T5990.d 1497：体力封顶 99999");
        std::cout << "  [T5990.d] 1497 恢复封顶 通过\n";
    }

    std::cout << "scenario_5990 PASS (J268 双方护盾总量回血族)" << std::endl;
    return 0;
}
