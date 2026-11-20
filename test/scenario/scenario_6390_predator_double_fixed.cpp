// 场景 6390 —— J306 天敌翻倍固伤族（1309，moves_lib/j_predator_double_fixed.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6390-6399）
//
// 断言口径：
//   - 1309（33350 深渊绝息 [1309]|[150]，纯链 cat2 pow140）：附加{0}点固定伤害，
//     **遇到天敌时翻倍**（is_predator = 对手固有属性克制自身固有属性、倍率 > 1）。
//   - 天敌构造：精灵固有属性 = 元素 id（1草 2水 3火 …）。水(2) 克 火(3)
//     → 己方=火(3)、对手=水(2) 即"遇到天敌"；双方同为水(2) → 非天敌。
//   - 观测面 = **审计槽**（插件写 plugin_storage[991309100+owner*2] = 本次实际附加
//     固伤额）做**精确**断言（150 / 300）；红伤本身 85%~100% 浮动 → 对手掉血只做
//     带断言（[amount, amount+20]），不拿掉血当精确量。
//   - 对照组（拍打 10004，无 1309 载体）：审计槽 0 + 掉血 < 15 → 证明掉血里的
//     固伤份额确实来自 1309，而非技能自身伤害。
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

constexpr int kCarrier1309 = 33350;   // 深渊绝息 [1309]|[150] 纯链 cat2 pow140
constexpr int kPaiDa = 10004;

constexpr int kElemWater = 2;   // 水（克制火）
constexpr int kElemFire = 3;    // 火（被水克制）

constexpr int kFixedKey = 991309100;   // +owner*2：本次实际附加的固伤额

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self(int self_elem, int atk) {
    ElfPet p = make_pet(1, "己方", {kCarrier1309, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    p.elementalAttributes = {self_elem, 0};   // 固有属性（天敌判定读这个）
    accuracy_100(p);
    return p;
}

ElfPet mk_self_plain(int self_elem) {
    ElfPet p = make_pet(1, "己方", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    p.elementalAttributes = {self_elem, 0};
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int foe_elem, int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    p.elementalAttributes = {foe_elem, 0};
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

    // ══ T6390.a 天敌档：己方=火(3)、对手=水(2) → 固伤 150×2 = 300 ══
    {
        ScenarioBattle b(make_party(mk_self(kElemFire, /*atk=*/1)),
                         make_party(mk_foe(kElemWater, /*hp=*/20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        run_skill(b, 0);
        const int drop = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(stor(ctx, kFixedKey + 0), 300,
                 "T6390.a 天敌 → 固伤翻倍 150×2=300（审计槽）");
        CHECK(drop >= 300 && drop <= 320,
              "T6390.a 对手掉血含 300 固伤（实测 " + std::to_string(drop) + "）");
    }

    // ══ T6390.b 非天敌档：双方同为水(2) → 固伤 150 不翻倍 ══
    {
        ScenarioBattle b(make_party(mk_self(kElemWater, /*atk=*/1)),
                         make_party(mk_foe(kElemWater, /*hp=*/20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        run_skill(b, 0);
        const int drop = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(stor(ctx, kFixedKey + 0), 150,
                 "T6390.b 非天敌 → 固伤 150 不翻倍（审计槽）");
        CHECK(drop >= 150 && drop <= 170,
              "T6390.b 对手掉血含 150 固伤（实测 " + std::to_string(drop) + "）");
    }

    // ══ T6390.c 对照组：无 1309 载体 → 无固伤附加（掉血只有技能自身伤害）══
    {
        ScenarioBattle b(make_party(mk_self_plain(kElemFire)),
                         make_party(mk_foe(kElemWater, /*hp=*/20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        run_skill(b, 0);
        const int drop = hp_before - ctx.getPet(1).hp;
        CHECK_EQ(stor(ctx, kFixedKey + 0), 0, "T6390.c 无 1309 → 审计槽 0");
        CHECK(drop < 15,
              "T6390.c 对照组掉血仅技能自身伤害（实测 " + std::to_string(drop) + "）");
    }

    std::cout << "scenario 6390 OK" << std::endl;
    return 0;
}
