// 场景 6080 —— J275 自身六槽增益·异况翻倍族（562，moves_lib/j_self_boost_anomaly_double.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6080-6089 → T6080.a-d 四块）
//
// 断言口径（逐槽快照，不看等级和）：
//   - 纯链载体 火焰战魂 22961 [562]|[0,1,1,1,1,0,2]（属性技）。
//   - 官方六槽 [0,1,1,1,1,0] → 引擎槽 {2,1,3,4} 各 +1（kStatMap{0,2,1,3,4,5}）。
//   - 门：对手处于异常码 {6}=2（烧伤）→ 翻倍；其他异常/无异常 → 原值。
//   - 审计 991562000+owner = 1/2；对手六槽不动。
//   - 起始 +6 满 cap：翻倍仍钳 6（坑集 §1：起始即满 cap 时"生效后不变"合法通过）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kHuoYan = 22961;   // 火焰战魂：[562]|[0,1,1,1,1,0,2]
constexpr int kZhenYi = 23993;   // 振翼强风：[562]|[0,2,0,2,0,0,8]（非对称槽，锁映射）
constexpr int kPaiDa  = 10004;   // 拍打：filler
constexpr int kMaxHp  = 99999;

constexpr int kAuditBase = 991562000;   // +owner：本次倍率（1/2）

// 期望：引擎槽 {1,2,3,4} 各 gain，槽 0/5 不变。
constexpr int kExpectSlots[4] = {1, 2, 3, 4};

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

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kHuoYan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet self_pet_zhenyi() {
    ElfPet p = make_pet(1, "己方", {kZhenYi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

bool is_target_slot(int slot) {
    for (int s : kExpectSlots) {
        if (s == slot) {
            return true;
        }
    }
    return false;
}

// 跑一局：可选给对手预置异常；返回 self 各槽等级（drive 后）。
void run_case(int foe_anomaly, int& mult_audit, int (&self_levels)[BattleContext::kAbilityLevelSlotCount],
              int (&foe_levels)[BattleContext::kAbilityLevelSlotCount]) {
    ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    if (foe_anomaly >= 0) {
        REQUIRE(apply_anomaly(&ctx, 1, foe_anomaly, 3, 0) == ApplyAnomalyResult::SUCCESS);
    }
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    mult_audit = audit_value(ctx, kAuditBase + 0);
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        self_levels[i] = ctx.ability_levels[0][i];
        foe_levels[i] = ctx.ability_levels[1][i];
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto check_levels = [](const int (&levels)[BattleContext::kAbilityLevelSlotCount], int gain,
                           const char* tag) {
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            if (is_target_slot(i)) {
                CHECK_EQ(levels[i], gain,
                         std::string(tag) + " 562：引擎槽 " + std::to_string(i) +
                             " 期望 +" + std::to_string(gain));
            } else {
                CHECK_EQ(levels[i], 0,
                         std::string(tag) + " 562：非目标槽 " + std::to_string(i) + " 不动");
            }
        }
    };

    // ══ T6080.a 无异常 → 原值（各 +1）══
    {
        int mult = 0;
        int self_lv[BattleContext::kAbilityLevelSlotCount] = {};
        int foe_lv[BattleContext::kAbilityLevelSlotCount] = {};
        run_case(-1, mult, self_lv, foe_lv);
        CHECK_EQ(mult, 1, "T6080.a 562：无异常 → 审计 1（不翻倍）");
        check_levels(self_lv, 1, "T6080.a");
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            CHECK_EQ(foe_lv[i], 0, "T6080.a 562：对手槽不动 " + std::to_string(i));
        }
        std::cout << "  [T6080.a] 562 无异常原值 通过\n";
    }

    // ══ T6080.b 对手烧伤(2) → 六槽翻倍（各 +2）══
    {
        int mult = 0;
        int self_lv[BattleContext::kAbilityLevelSlotCount] = {};
        int foe_lv[BattleContext::kAbilityLevelSlotCount] = {};
        run_case(2, mult, self_lv, foe_lv);
        CHECK_EQ(mult, 2, "T6080.b 562：对手烧伤 → 审计 2（翻倍）");
        check_levels(self_lv, 2, "T6080.b");
        std::cout << "  [T6080.b] 562 异况翻倍 通过\n";
    }

    // ══ T6080.c 对手中毒(1) ≠ 门码 2 → 不翻倍 ══
    {
        int mult = 0;
        int self_lv[BattleContext::kAbilityLevelSlotCount] = {};
        int foe_lv[BattleContext::kAbilityLevelSlotCount] = {};
        run_case(1, mult, self_lv, foe_lv);
        CHECK_EQ(mult, 1, "T6080.c 562：异常码不匹配 → 审计 1");
        check_levels(self_lv, 1, "T6080.c");
        std::cout << "  [T6080.c] 562 精确异常码门 通过\n";
    }

    // ══ T6080.d 起始 +6 满 cap + 翻倍门开 → 钳 6（合法通过态）══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(apply_anomaly(&ctx, 1, 2, 3, 0) == ApplyAnomalyResult::SUCCESS);
        for (int s : kExpectSlots) {
            ctx.ability_levels[0][s] = 6;
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 2, "T6080.d 562：翻倍门仍触发");
        for (int s : kExpectSlots) {
            CHECK_EQ(ctx.ability_levels[0][s], 6,
                     "T6080.d 562：满 cap 槽 " + std::to_string(s) + " 钳住");
        }
        std::cout << "  [T6080.d] 562 满 cap 钳位 通过\n";
    }

    // ══ T6080.e 非对称槽锁官方→引擎映射：[0,2,0,2,0,0] → 引擎 {2:+2, 3:+2} ══
    // 若实现直传官方下标（漏 kStatMap），会落到引擎槽 {1,+2}/{3,+2} → 本块转红。
    {
        ScenarioBattle b(make_party(self_pet_zhenyi()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(audit_value(ctx, kAuditBase + 0), 1, "T6080.e 562：无异常 → 不翻倍");
        CHECK_EQ(ctx.ability_levels[0][2], 2, "T6080.e 562：官方防(1) → 引擎槽2 +2");
        CHECK_EQ(ctx.ability_levels[0][3], 2, "T6080.e 562：官方特防(3) → 引擎槽3 +2");
        CHECK_EQ(ctx.ability_levels[0][0], 0, "T6080.e 562：攻槽不动");
        CHECK_EQ(ctx.ability_levels[0][1], 0, "T6080.e 562：引擎槽1（特攻）不动");
        CHECK_EQ(ctx.ability_levels[0][4], 0, "T6080.e 562：速槽不动");
        CHECK_EQ(ctx.ability_levels[0][5], 0, "T6080.e 562：命中槽不动");
        std::cout << "  [T6080.e] 562 官方码序映射 通过\n";
    }

    std::cout << "scenario_6080 PASS (J275 自身六槽增益·异况翻倍族)" << std::endl;
    return 0;
}
