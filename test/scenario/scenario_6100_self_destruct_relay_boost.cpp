// 场景 6100 —— J277 自爆传功下只族（59，moves_lib/j_self_destruct_relay_boost.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6100-6109 → T6100.a-b 两块）
//
// 断言口径（逐槽快照）：
//   - 载体 灵魂附体 20175 [59]|[2,3]（属性技）→ 官方特攻(2)/特防(3) → 引擎槽 1/3 各 +1；
//     载体 传承 20749 [59]|[4,5] → 官方速(4)/命中(5) → 引擎槽 4/5 各 +1（锁映射）。
//   - 自爆：首位宠物 hp==0、on_stage 已切换；下一只出场宠 hp>0。
//   - 审计 991059000+0 = 1（传功一次）；非目标槽全 0。
#include <any>
#include <iostream>
#include <string>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kLingHun = 20175;   // 灵魂附体：[59]|[2,3]
constexpr int kChuanCheng = 20749;  // 传承：[59]|[4,5]
constexpr int kPaiDa = 10004;     // 拍打：filler
constexpr int kMaxHp = 99999;

constexpr int kAuditBase = 991059000;   // +owner：1=已传功

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

ElfPet self_pet(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 返回下一只出战宠的六槽（drive 后）。
void run_case(int carrier, int (&levels)[BattleContext::kAbilityLevelSlotCount],
              int& audit, int& old_hp, int& new_on_stage) {
    ScenarioBattle b(make_party(self_pet(carrier)), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    // 自爆后进入补位状态：提交 CHOOSE_PET 让"下一只出战"真正登场。
    b.submit_operation(0, BattleFsm::ActionType::CHOOSE_PET, 1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    audit = audit_value(ctx, kAuditBase + 0);
    old_hp = ctx.seerRobot[0].elfPets[0].hp;
    new_on_stage = ctx.on_stage[0];
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        levels[i] = ctx.ability_levels[0][i];
    }
}

void check_slots(const int (&levels)[BattleContext::kAbilityLevelSlotCount], int s1, int s2,
                 const char* tag) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        const int want = (i == s1 || i == s2) ? 1 : 0;
        CHECK_EQ(levels[i], want,
                 std::string(tag) + " 59：引擎槽 " + std::to_string(i) + " 期望 +" +
                     std::to_string(want));
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6100.a 灵魂附体 [2,3] → 引擎槽 1/3 各 +1 ══
    {
        int levels[BattleContext::kAbilityLevelSlotCount] = {};
        int audit = -1;
        int old_hp = 0;
        int next_slot = -1;
        run_case(kLingHun, levels, audit, old_hp, next_slot);
        CHECK_EQ(old_hp, 0, "T6100.a 59：首位自爆 hp=0");
        CHECK(next_slot != 0, "T6100.a 59：已切换到下一只出战");
        CHECK(next_slot > 0 && next_slot < 6, "T6100.a 59：下一只槽位合法");
        CHECK_EQ(audit, 1, "T6100.a 59：审计已传功 1");
        check_slots(levels, 1, 3, "T6100.a");
        std::cout << "  [T6100.a] 59 自爆传功 通过 (next_slot=" << next_slot << ")\n";
    }

    // ══ T6100.b 传承 [4,5] → 引擎槽 4/5 各 +1（锁官方码序映射）══
    {
        int levels[BattleContext::kAbilityLevelSlotCount] = {};
        int audit = -1;
        int old_hp = 0;
        int next_slot = -1;
        run_case(kChuanCheng, levels, audit, old_hp, next_slot);
        CHECK_EQ(old_hp, 0, "T6100.b 59：首位自爆 hp=0");
        CHECK(next_slot != 0, "T6100.b 59：已切换到下一只出战");
        CHECK_EQ(audit, 1, "T6100.b 59：审计已传功 1");
        check_slots(levels, 4, 5, "T6100.b");
        std::cout << "  [T6100.b] 59 官方码序映射 通过\n";
    }

    std::cout << "scenario_6100 PASS (J277 自爆传功下只族)" << std::endl;
    return 0;
}
