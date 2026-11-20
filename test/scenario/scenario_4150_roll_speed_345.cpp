#include <utils/sbs_rand.h>
// 场景 4150 —— K259 每次用技概率速度+1族（345，soul_lib/k259_roll_speed.cpp）
//（2026-10-21，worktree ../agents/k259；号段 4150-4159 → T4150-T4152 三块）
//
// 断言口径（345「每次使用技能有10%的概率提升速度等级1」）：
//   - T4150 合成行 [100,...]：100% 档（roll_percent(100) 直接真、不掷 rand）→
//     自己用技后速度等级 +1；审计槽 = 1（触发）。
//   - T4151 合成行 [0,...]：0% 档 → 不触发（审计槽 = 0，speed 等级不变）。
//   - T4152 真实行 [10,7,7,7,7,8,7]：审计槽记录本次掷点结果（0/1）——
//     10% 档不靠随机断言"必然触发"，只断言**掷点发生过且结果被记账**；
//     再用 srand 钉种子做一次可复现的落点断言（K193 827 探针钉种子同款）。
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

constexpr int kJi = 10001;     // 撞击：攻击载体
constexpr int kSpeedSlot = 4;  // 能力等级槽 4 = 速度
constexpr int kRollAuditBase = 991459000;   // +owner：本次掷点结果（1/0）

ElfPet mark_pet(const std::vector<int>& row) {
    ElfPet p = make_pet(1, "印主", {kJi, kJi, kJi, kJi, kJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/99999,
                        /*soul_mark=*/SoulMark(345, "K259 魂印", "",
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

void my_strike(ScenarioBattle& b) {
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

    // ══ T4150 100% 档：必触发 + 速度+1 ══
    {
        ScenarioBattle b(make_party(mark_pet({100, 7, 7, 7, 7, 8, 7})),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(ctx.ability_levels[0][kSpeedSlot] == 0);
        my_strike(b);
        CHECK_EQ(audit_int(ctx, kRollAuditBase + 0), 1,
                 "T4150 345：100% 档掷点结果 = 触发");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 1,
                 "T4150 345：100% 档 → 自身速度等级 +1");
        std::cout << "  [T4150] 100% 触发 通过\n";
    }

    // ══ T4151 0% 档：不触发 ══
    {
        ScenarioBattle b(make_party(mark_pet({0, 7, 7, 7, 7, 8, 7})),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        my_strike(b);
        CHECK_EQ(audit_int(ctx, kRollAuditBase + 0), 0,
                 "T4151 345：0% 档掷点结果 = 未触发");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "T4151 345：0% 档 → 自身速度等级不变");
        std::cout << "  [T4151] 0% 不触发 通过\n";
    }

    // ══ T4152 真实行 [10,...]：掷点被记账（0/1 二值） + 种子钉死的可复现落点 ══
    {
        ScenarioBattle b(make_party(mark_pet({10, 7, 7, 7, 7, 8, 7})),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        sbs_srand(3);   // 钉种子 → rand 序列可复现
        my_strike(b);
        const int rolled = audit_int(ctx, kRollAuditBase + 0);
        CHECK(rolled == 0 || rolled == 1,
              "T4152 345：真实行掷点结果被记账（0/1）");
        // 10% 档下"速度等级 == 掷点结果"（触发即 +1，未触发即 0）。
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], rolled,
                 "T4152 345：速度等级增量与掷点结果一致");
        std::cout << "  [T4152] 真实行记账 通过（rolled=" << rolled << "）\n";
    }

    // ══ T4153 门控：只有**自己**用技才掷点（对手用技 → 印主速度不变）════════
    {
        ScenarioBattle b(make_party(mark_pet({100, 7, 7, 7, 7, 8, 7})),
                         make_party(plain_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        // 印主 NONE（不用技），对手 SELECT_SKILL（对手用技）→ 印主不应加速。
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        CHECK_EQ(audit_int(ctx, kRollAuditBase + 0), -1,
                 "T4153 345：对手用技 → 印主侧未掷点（审计槽未动）");
        CHECK_EQ(ctx.ability_levels[0][kSpeedSlot], 0,
                 "T4153 345：对手用技 → 印主速度等级不变");
        std::cout << "  [T4153] 使用者门对照 通过\n";
    }

    std::cout << "scenario_4150 PASS (K259 每次用技概率速度+1族)" << std::endl;
    return 0;
}
