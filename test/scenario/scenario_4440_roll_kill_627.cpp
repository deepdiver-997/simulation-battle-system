// 场景 4440 —— K288 概率秒杀族（627，soul_lib/k288_roll_kill.cpp）
//（2026-10-21，worktree ../agents/k288；号段 4440-4449 → T4440-T4441 两块）
//
// 断言口径（627「4%秒杀对手」）：
//   - T4440 真实行 [4,0]：审计槽申报值 = 4（**无随机精确断言**，F9 口径）——4% 太
//     稀疏，落地与否不靠随机断言。
//   - T4441 合成行 [100,0]：100% 档 → 印主用攻击技后**对手被秒杀**（hp=0）。
//     对照：属性技不触发（攻击技门）。
//
// ⚠️ 场景构造口径：按 **DB args 原样**传（不补 {0,0} 前缀）——实现侧走
//   owned_int_args 原序读（K257 实现头注「行参读取路径」记档）。
#include <cstdlib>
#include <iostream>
#include <vector>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJi = 10001;     // 撞击：攻击技载体
constexpr int kSuoTou = 20004; // 瞪眼：属性技载体（effect 5=改对手等级）
constexpr int kChanceAuditBase = 991288000;   // +owner：本次申报概率%（审计槽）

// 印主：行参可配（真实行 4% / 合成行 100%）。
ElfPet mark_pet(const std::vector<int>& row, int skill_id = kJi) {
    ElfPet p = make_pet(1, "印主", {skill_id, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(627, "K288 魂印", "",
                                               EffectArgs(row)));
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

ElfPet plain_pet() {
    ElfPet p = make_pet(2, "对手", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    for (Skills& s : p.skills) {
        s.must_hit = 0;
        s.accuracy = 100;
    }
    return p;
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T4440 真实行：审计槽申报值 = 4 ═══════════════════════════════════════
    {
        ScenarioBattle b(make_party(mark_pet({4, 0})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(audit_int(ctx, kChanceAuditBase + 0), 4,
                 "T4440 627：真实行申报值 = db[0] = 4（百分比口径，非千分比）");
        std::cout << "  [T4440] 真实行审计 通过\n";
    }

    // ══ T4441 合成行 100%：攻击技 → 对手被秒杀；属性技 → 不触发 ═════════════
    {
        ScenarioBattle b(make_party(mark_pet({100, 0})), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK_EQ(ctx.getPet(1).hp, 0,
                 "T4441 627：100% 档 → 印主用攻击技后对手被秒杀");
        std::cout << "  [T4441] 100% 秒杀 通过\n";
    }
    {
        ScenarioBattle b(make_party(mark_pet({100, 0}, kSuoTou)), make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
        b.drive();
        CHECK(ctx.getPet(1).hp > 0,
              "T4441 627：用属性技 → 不触发秒杀（攻击技门）");
        std::cout << "  [T4441b] 属性技门对照 通过\n";
    }

    std::cout << "scenario_4440 PASS (K288 概率秒杀族)" << std::endl;
    return 0;
}
