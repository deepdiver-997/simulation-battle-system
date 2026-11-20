// 场景 5120 —— J190 强化翻倍/镜像族（1177 / 1856，moves_lib/j_boost_double_mirror.cpp）
//（2026-10-22，worktree ../agents/j190；号段 5120-5129）
//
// 断言口径：
//   - 1177（极寒冲击 34794 纯链 [1177]）：开局槽0=+2、槽1=+5 → 翻倍后槽0=+4、
//     槽1=+6（5×2=10 被 AT_CAP 钳到 6；"起始接近满 cap 时到顶"为合法通过态）。
//     六槽逐槽快照比对，不看等级和。对照：全 0 → 不触发（六槽保持 0）。
//   - 1856（垂宇龙吟 28083 链 [1856,1842,58]|[1,4,300,400,2]）：开局槽0=-2、槽2=+1
//     → 全属性+1 后槽0=-1、槽2=+2；再对"强化前为负"的槽0 补 +2 → 槽0=+1；
//     其余槽各 +1（槽1/3/4/5=+1）。对照：全 0 → 六槽全 +1（无镜像段）。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa      = 10004;  // 拍打：物 40（填充刀）
constexpr int kJiHan      = 34794;  // [1177]（翻倍）
constexpr int kChuiYu     = 28083;  // [1856,1842,58]|[1,4,300,400,2]（全属性+1/镜像）
constexpr int kSlots      = 6;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(int skill) {
    ElfPet p = make_pet(1, "己方", {skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/0, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

void drive_one(ScenarioBattle& b) {
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

    // ══ T5120 1177 正等级翻倍 ══
    {
        {   // (a) 槽0=2 / 槽1=5 → 4 / 6（cap）
            ScenarioBattle b(make_party(mk_self(kJiHan)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[0][0] = 2;
            ctx.ability_levels[0][1] = 5;
            drive_one(b);
            for (int slot = 0; slot < kSlots; ++slot) {
                const int want = slot == 0 ? 4 : (slot == 1 ? 6 : 0);
                CHECK_EQ(ctx.ability_levels[0][slot], want,
                         "T5120.a 1177：槽 " + std::to_string(slot) + " 翻倍后期望 " +
                             std::to_string(want) + "（实测 " +
                             std::to_string(ctx.ability_levels[0][slot]) + "）");
            }
        }
        {   // (b) 对照：全 0 → 不触发
            ScenarioBattle b(make_party(mk_self(kJiHan)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            drive_one(b);
            for (int slot = 0; slot < kSlots; ++slot) {
                CHECK_EQ(ctx.ability_levels[0][slot], 0,
                         "T5120.b 1177 对照：无提升时槽 " + std::to_string(slot) +
                             " 保持 0（实测 " +
                             std::to_string(ctx.ability_levels[0][slot]) + "）");
            }
        }
        std::cout << "  [T5120] 1177 正等级翻倍 + 对照 通过\n";
    }

    // ══ T5120 1856 全属性+{0} + 负槽镜像 ══
    {
        {   // (a) 槽0=-2 / 槽2=+1 → 全+1 → 槽0 补镜像 +2 → 槽0=+1；其余逐槽 +1
            ScenarioBattle b(make_party(mk_self(kChuiYu)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.ability_levels[0][0] = -2;
            ctx.ability_levels[0][2] = 1;
            drive_one(b);
            const int want[kSlots] = {1, 1, 2, 1, 1, 1};
            for (int slot = 0; slot < kSlots; ++slot) {
                CHECK_EQ(ctx.ability_levels[0][slot], want[slot],
                         "T5120.c 1856：槽 " + std::to_string(slot) + " 期望 " +
                             std::to_string(want[slot]) + "（实测 " +
                             std::to_string(ctx.ability_levels[0][slot]) + "）");
            }
        }
        {   // (b) 对照：无负槽 → 六槽全 +1（镜像段不执行）
            ScenarioBattle b(make_party(mk_self(kChuiYu)), make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            drive_one(b);
            for (int slot = 0; slot < kSlots; ++slot) {
                CHECK_EQ(ctx.ability_levels[0][slot], 1,
                         "T5120.d 1856 对照：槽 " + std::to_string(slot) +
                             " = +1（实测 " +
                             std::to_string(ctx.ability_levels[0][slot]) + "）");
            }
        }
        std::cout << "  [T5120] 1856 全属性/负槽镜像 + 对照 通过\n";
    }

    std::cout << "scenario_5120 PASS (J190 强化翻倍/镜像族)" << std::endl;
    return 0;
}
