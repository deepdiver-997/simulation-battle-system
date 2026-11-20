// 场景 2260 —— J131 速度附加累积族（869，moves_lib/j_speed_rider_accum.cpp）
//（2026-10-04，worktree ../agents/j131；号段 2260-2269 → T2260-T2262 三块）
//
// 断言口径：
//   - 纯链载体 异空反冲 33537 [869]→[30,10,50]（物130，施放即红伤=首跳触发）；
//     make_pet speed=200 → rider 梯度 60→80→100→100（200×30/40/50/50，封顶后持平）；
//   - 红伤 final 用 ws.raw_attack_damage[0]（battleFsm:898 写 resolvedDamage.final、
//     每次出手清零）做**精确**差账：foe_hp_after = foe_hp_before − raw − rider；
//   - 累积梯度走审计槽 plugin_storage[990086910+0]（F9 782 口径）无随机断言；
//   - 速度取值面：setup 直降自身速度 -1（视图 200×2/3=133）→ 首跳 rider=133×30/100
//     =39（整除）——钉死"视图面吃升降"与整除口径；
//   - 重武装不重置：第二次施放后槽位 40→50（若武装重置为 30 则会是 40——判别性）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kYiKong = 33537;   // 异空反冲：物130 纯链 [869] → [30,10,50]
constexpr int kPaDa   = 10004;   // 拍打：物攻40（filler）

constexpr int kAudit869Self = 990086910 + 0;   // 审计槽（owner=0）
constexpr int kSlotSpd = 4;                    // 引擎速度等级槽（0攻1特攻2防3特防4速5命中）

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 200) {
    ElfPet p = make_pet(1, "己方", skills, speed, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    always_hit(p);
    return p;
}

void both_act(ScenarioBattle& b, int self_slot, int foe_slot = 0) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, self_slot);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, foe_slot);
    b.drive();
}

int audit(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAudit869Self);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2260 累积梯度 60→80→100→100（封顶持平）+ 审计槽 30→40→50→50 ══
    {
        ScenarioBattle b(make_party(self_pet({kYiKong, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        int foe_hp = 99999;
        const int riders[4] = {60, 80, 100, 100};   // 200×30/40/50/50
        const int slots[4] = {40, 50, 50, 50};      // 触发后槽位
        for (int r = 0; r < 4; ++r) {
            const int slot = r == 0 ? 0 : 1;   // R1 用技武装+触发，R2-R4 拍打触发
            both_act(b, slot);
            const int raw = ctx.ws.raw_attack_damage[0];
            foe_hp -= raw + riders[r];
            char msg[96];
            std::snprintf(msg, sizeof(msg), "T2260 R%d：rider=%d 精确落地（r%d=%d）", r + 1,
                          riders[r], r + 1, raw);
            CHECK_EQ(ctx.getPet(1).hp, foe_hp, msg);
            std::snprintf(msg, sizeof(msg), "T2260 R%d：审计槽=%d（量累积）", r + 1, slots[r]);
            CHECK_EQ(audit(ctx), slots[r], msg);
        }
        std::cout << "  [T2260] 869 累积梯度/封顶 通过\n";
    }

    // ══ T2261 速度视图面：自身速度 -1 → 视图 133 → 首跳 rider=39 ══
    {
        ScenarioBattle b(make_party(self_pet({kYiKong, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        REQUIRE(stat_change(&ctx, 0, kSlotSpd, -1) == StatChangeResult::SUCCESS);
        both_act(b, 0);
        const int raw = ctx.ws.raw_attack_damage[0];
        CHECK_EQ(ctx.getPet(1).hp, 99999 - raw - 39,
                 "T2261 869：速度视图 133（200×2/3）→ rider=133×30/100=39");
        CHECK_EQ(audit(ctx), 40, "T2261 869：触发后槽位 40");
        std::cout << "  [T2261] 869 速度视图面 通过\n";
    }

    // ══ T2262 重武装不重置（累积跨用技持续——文件头记档口径的判别性断言）══
    {
        ScenarioBattle b(make_party(self_pet({kYiKong, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 首跳：30→触发→40
        CHECK_EQ(audit(ctx), 40, "T2262 869：R1 触发后槽位 40");
        both_act(b, 0);   // R2 再次施放：武装不重置（若重置为 30 则触发后=40）
        CHECK_EQ(audit(ctx), 50, "T2262 869：重武装不重置（40+10=50，判别 40）");
        std::cout << "  [T2262] 869 重武装不重置 通过\n";
    }

    std::cout << "scenario_2260 PASS (J131 速度附加累积族)" << std::endl;
    return 0;
}
