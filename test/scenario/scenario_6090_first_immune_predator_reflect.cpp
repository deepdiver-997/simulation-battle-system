// 场景 6090 —— J276 先手免伤·天敌反伤族（1511，moves_lib/j_first_immune_predator_reflect.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6090-6099 → T6090.a-c 三块）
//
// 断言口径：
//   - 载体 踏蔓先登 28457 [610,1511]|[1,50]（属性技；1511 pct=50）。
//   - 天敌局：自身水(2) / 对手草(1) → is_predator(self) 为真；对手拍打红伤被 BLOCK
//     归零（自身 hp 绝对值不变），反射 = 免伤量×50%（审计精确对拍），对手 hp 减少
//     恰等于反射量（PERCENT_VALUE 粉伤落到本体）。
//   - 非天敌局：自身水(2) / 对手火(4) → 只免伤、反射 0、对手 hp 不变。
//   - 后手局：自身速度 100 / 对手 300（非天敌，610 不加先制）→ 1511 不注册，
//     对手先手红伤落地、自身 hp 下降。
#include <any>
#include <iostream>

#include <entities/elemental-attributes.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kTaMan   = 28457;   // 踏蔓先登：610+1511|[1,50]
constexpr int kPaiDa   = 10004;   // 拍打：对手攻击
constexpr int kMaxHp   = 99999;

constexpr int kImmuneBase = 991511000;   // +owner：本次免伤量
constexpr int kReflectBase = 991511100;  // +owner：本次反射量

constexpr int kWater = 2;
constexpr int kGrass = 1;
constexpr int kFire  = 4;

int audit_value(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : -1;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(int self_speed) {
    ElfPet p = make_pet(1, "己方", {kTaMan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        self_speed, /*atk=*/1, kMaxHp);
    p.elementalAttributes[0] = kWater;
    p.elementalAttributes[1] = 0;
    always_hit(p);
    return p;
}

ElfPet foe_pet(int element, int speed) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1000, kMaxHp);
    p.elementalAttributes[0] = element;
    p.elementalAttributes[1] = 0;
    always_hit(p);
    return p;
}

struct Outcome {
    int self_loss = 0;
    int foe_loss = 0;
    int immune = 0;
    int reflect = 0;
};

Outcome run_case(int foe_element, int self_speed, int foe_speed) {
    Outcome out;
    ScenarioBattle b(make_party(self_pet(self_speed)), make_party(foe_pet(foe_element, foe_speed)));
    b.start();
    BattleContext& ctx = b.ctx();
    // 前置：确认天敌关系与构造一致（is_predator 读固有属性表）。
    const double mult = core_api().restraint_multiplier(
        ctx.getPet(1).elementalAttributes.data(), ctx.getPet(0).elementalAttributes.data());
    if (foe_element == kGrass) {
        REQUIRE(mult > 1.0);
    } else {
        REQUIRE(mult <= 1.0);
    }
    const int self_hp0 = ctx.getPet(0).hp;
    const int foe_hp0 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    out.self_loss = self_hp0 - ctx.getPet(0).hp;
    out.foe_loss = foe_hp0 - ctx.getPet(1).hp;
    out.immune = audit_value(ctx, kImmuneBase + 0);
    out.reflect = audit_value(ctx, kReflectBase + 0);
    return out;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6090.a 天敌局：先手免伤 + 反射 50% 百分比伤害 ══
    {
        const Outcome o = run_case(kGrass, /*self_speed=*/300, /*foe_speed=*/100);
        CHECK_EQ(o.self_loss, 0, "T6090.a 1511：先手免伤 → 自身 hp 不变");
        CHECK(o.immune > 0, "T6090.a 1511：审计免伤量 > 0");
        CHECK_EQ(o.reflect, o.immune * 50 / 100, "T6090.a 1511：反射量 = 免伤×50%");
        CHECK_EQ(o.foe_loss, o.reflect, "T6090.a 1511：对手掉血恰等于反射量");
        std::cout << "  [T6090.a] 1511 天敌反射 通过 (immune=" << o.immune
                  << " reflect=" << o.reflect << ")\n";
    }

    // ══ T6090.b 非天敌局：只免伤、不反射 ══
    {
        const Outcome o = run_case(kFire, /*self_speed=*/300, /*foe_speed=*/100);
        CHECK_EQ(o.self_loss, 0, "T6090.b 1511：先手免伤 → 自身 hp 不变");
        CHECK(o.immune > 0, "T6090.b 1511：审计免伤量 > 0");
        CHECK_EQ(o.reflect, 0, "T6090.b 1511：非天敌 → 反射 0");
        CHECK_EQ(o.foe_loss, 0, "T6090.b 1511：非天敌 → 对手 hp 不变");
        std::cout << "  [T6090.b] 1511 非天敌只免伤 通过\n";
    }

    // ══ T6090.c 后手局：1511 不注册（对手先手红伤落地）══
    {
        const Outcome o = run_case(kFire, /*self_speed=*/100, /*foe_speed=*/300);
        CHECK(o.self_loss > 0, "T6090.c 1511：后手 → 对手红伤落地");
        CHECK_EQ(o.immune, -1, "T6090.c 1511：后手不注册免伤（审计缺省 -1）");
        CHECK_EQ(o.foe_loss, 0, "T6090.c 1511：后手不反射");
        std::cout << "  [T6090.c] 1511 后手不触发 通过 (self_loss=" << o.self_loss << ")\n";
    }

    std::cout << "scenario_6090 PASS (J276 先手免伤·天敌反伤族)" << std::endl;
    return 0;
}
