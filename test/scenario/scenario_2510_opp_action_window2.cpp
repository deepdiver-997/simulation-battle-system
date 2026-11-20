// 场景 2510 —— J160 对手行为窗族二（1118 / 1089，moves_lib/j_opp_action_window2.cpp）
//（2026-10-04，worktree ../agents/j160；号段 2510-2519 → T2510-T2512 三块）
//
// 断言口径：
//   - 1118 走纯链载体 废油瓦斯 37271 [3,100,10,1]（主干 100% 确定档）：对手用攻击技
//     → 中码 10；对手用属性技（27995 盛夏相逢，1259 本 worktree 未实现=哑链）
//     → 六槽各 -1（level_sum=-6）；
//   - 1089 走 大海守望 25616 [3,1] / 灵魂绝响 25287 [3,2]：异常**计数**断言
//     （0..9 中恰 1/2 种激活）——不依赖具体落码，免种子校准。
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

constexpr int kFeiYou  = 37271;   // 废油瓦斯：特140 纯链 [1118] → [3,100,10,1]
constexpr int kDaHai   = 25616;   // 大海守望：属 [1089] → [3,1]
constexpr int kLingHun = 25287;   // 灵魂绝响：属 [1089] → [3,2]
constexpr int kPaDa    = 10004;   // 拍打：对手攻击技载体
constexpr int kShengXia= 27995;   // 盛夏相逢：属（1259 本 worktree 未实现=哑链，属性技载体）

constexpr int kCode10 = 10;

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, /*speed=*/300, /*atk=*/1, /*hp=*/99999);
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

int level_sum(BattleContext& ctx, int side) {
    int sum = 0;
    for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
        sum += ctx.ability_levels[side][i];
    }
    return sum;
}

// 对手 0..9 异常码激活计数。
int foe_anomaly_count(BattleContext& ctx) {
    int n = 0;
    for (int code = 0; code <= 9; ++code) {
        if (ctx.has_active_abnormal_status(1, code)) {
            ++n;
        }
    }
    return n;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T2510 1118 攻击技分支（废油瓦斯 [3,100,10,1]）══
    {
        ScenarioBattle b(make_party(self_pet({kFeiYou, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 我施放武装窗 → 对手用攻击技 → 100% 中码 10
        CHECK(ctx.has_active_abnormal_status(1, kCode10),
              "T2510 1118：对手用攻击技 → 100% 异常码 10");
        std::cout << "  [T2510] 1118 攻击技分支 通过\n";
    }

    // ══ T2511 1118 属性技分支 → 全属性 -1（对手载体=哑链属性技）══
    {
        ScenarioBattle b(make_party(self_pet({kFeiYou, kPaDa, kPaDa, kPaDa, kPaDa})),
                         make_party(foe_pet({kShengXia, kPaDa, kPaDa, kPaDa, kPaDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        both_act(b, 0);   // R1 我施放武装窗 → 对手用属性技 → 六槽各 -1
        CHECK_EQ(level_sum(ctx, 1), -6, "T2511 1118：对手用属性技 → 全属性 -1（六槽）");
        std::cout << "  [T2511] 1118 属性技分支 通过\n";
    }

    // ══ T2512 1089 随机异常计数（大海守望 [3,1] / 灵魂绝响 [3,2]）══
    {
        {   // kinds=1：恰 1 种
            ScenarioBattle b(make_party(self_pet({kDaHai, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            both_act(b, 0);
            CHECK_EQ(foe_anomaly_count(ctx), 1, "T2512.a 1089：随机附加恰 1 种异常");
        }
        {   // kinds=2：恰 2 种（不重复）
            ScenarioBattle b(make_party(self_pet({kLingHun, kPaDa, kPaDa, kPaDa, kPaDa})),
                             make_party(foe_pet({kPaDa, kPaDa, kPaDa, kPaDa, kPaDa})));
            b.start();
            BattleContext& ctx = b.ctx();
            both_act(b, 0);
            CHECK_EQ(foe_anomaly_count(ctx), 2, "T2512.b 1089：随机附加恰 2 种不同异常");
        }
        std::cout << "  [T2512] 1089 随机异常计数 通过\n";
    }

    std::cout << "scenario_2510 PASS (J160 对手行为窗族二)" << std::endl;
    return 0;
}
