// 场景 5740 —— J243 命中全属性降窗族（965，moves_lib/j_hit_all_down_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5740-5749）
//
// 断言口径（精确六槽快照 + 窗界）：
//   - 载体 28016 祷灵祭坛 [965]|[3,1]（属性技，纯链）：R1 施放 → 3 回合窗（R1-R3）。
//   - T5740.a：R1 当次是属性技 → 不触发，对手六槽全 0（窗 R1-R3 起算）。
//   - T5740.b：R2 自身用**另一个属性技**（20017 高速移动，自身速度+2，不碰对手等级）
//     → 不触发，六槽保持 0。
//   - T5740.c：R3 自身用**攻击技**命中 → 对手六槽全部 -1（逐槽快照，不看等级和）。
//   - T5740.d：R4 攻击技再命中 → 窗已过期（R1-R3）→ 六槽仍 -1（不叠加）。
//   - T5740.f：同结构但攻击技 accuracy=0 → miss，窗内命中门拦下 → 六槽保持 0。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/seer-robot.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <fsm/state.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;   // 拍打：物 40
constexpr int kWindow  = 28016;   // [965]|[3,1] 祷灵祭坛：属性技
constexpr int kSelfBuff = 20017;  // 高速移动：自身速度+2（不碰对手等级）
constexpr int kMaxHp   = 99999;

ElfPet mk_self(bool miss_attack) {
    ElfPet p = make_pet(1, "己方", {kWindow, kPaiDa, kSelfBuff, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    if (miss_attack) {
        p.skills[1].accuracy = 0;   // 槽 1=拍打：必 miss
        p.skills[1].must_hit = 0;
    }
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/kMaxHp);
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
    return p;
}

int foe_level(BattleContext& ctx, int slot) {
    return ctx.ability_levels[1][slot];
}

void round_with(ScenarioBattle& b, int self_skill_index) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_skill_index);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
}

void check_all_slots(BattleContext& ctx, int expected, const std::string& label) {
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        CHECK_EQ(foe_level(ctx, i), expected,
                 label + " 对手槽 " + std::to_string(i) + " 等级");
    }
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5740.a-d 窗内命中触发 / 属性技不触发 / 窗过期 ══
    {
        ScenarioBattle b(make_party(mk_self(false)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();

        round_with(b, 0);   // R1：属性技开窗（当次不触发）
        check_all_slots(ctx, 0, "T5740.a R1 属性技开窗");

        round_with(b, 2);   // R2：另一个属性技 → 不触发
        check_all_slots(ctx, 0, "T5740.b R2 属性技不触发");

        round_with(b, 1);   // R3：攻击技命中 → 六槽 -1
        check_all_slots(ctx, -1, "T5740.c R3 攻击命中");

        round_with(b, 3);   // R4：窗过期（R1-R3）→ 不再降
        check_all_slots(ctx, -1, "T5740.d R4 窗过期");
        std::cout << "  [T5740.a-d] 六槽快照 0 → 0 → -1 → -1（窗 R1-R3）通过\n";
    }

    // ══ T5740.f 窗内攻击 miss → 不触发 ══
    {
        ScenarioBattle b(make_party(mk_self(true)), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        round_with(b, 0);   // R1 开窗
        round_with(b, 1);   // R2 攻击必 miss
        check_all_slots(ctx, 0, "T5740.f miss 不触发");
        std::cout << "  [T5740.f] 窗内 miss → 六槽保持 0 通过\n";
    }

    std::cout << "scenario_5740 PASS (965 命中全属性降窗)" << std::endl;
    return 0;
}
