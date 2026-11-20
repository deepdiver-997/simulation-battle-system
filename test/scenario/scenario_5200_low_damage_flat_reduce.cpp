// 场景 5200 —— J197 低伤→下N次点减伤族（1202，moves_lib/j_low_damage_flat_reduce.cpp）
//（2026-10-22，号段 5200-5209）
//
// 断言口径：
//   - 1202（冰脉护手 33740 纯链 [1202]|[300,1,300]，物 140）：
//     · (a) 己方 atk=1 → 本刀伤害 1-2 < 300 → 授 1 次点数减伤 300；随后对手
//       atk=300 拍打（基础 [87,102]）被扣到 0 → 己方 hp 不变；次数消耗到 0。
//     · (b) 己方 atk=300 → 本刀伤害 [357,420] ≥ 300 → 不授次数；对手同一刀
//       正常落地 [87,102]（绝对值断言，不用"与对照相等"）。
//     · (c) 对照：己方用填充刀（不施放载体）→ 无次数；对手伤害 [87,102]。
//   - 审计槽 990120201+self = 本刀实际伤害（证明检测相位真的走到）；990120200+self =
//     剩余次数（-1 = 键不存在）。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa    = 10004;  // 拍打：物 40（填充/对手攻击）
constexpr int kBingMai  = 33740;  // [1202]|[300,1,300]（物 140）
constexpr int kDamageAudit = 990120201;
constexpr int kChargeAudit = 990120200;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int first_skill, int atk) {
    ElfPet p = make_pet(1, "己方", {first_skill, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int storage_int(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return -1;
    }
    if (const int* p = std::any_cast<int>(&it->second)) {
        return *p;
    }
    return -1;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5200 1202 低伤门 → 下 N 次点数减伤 ══
    {
        {   // (a) 低伤触发：本刀 <300 → 授 1 次 300 点减伤 → 对手 87-102 被扣到 0
            ScenarioBattle b(make_party(mk_owner(kBingMai, /*atk=*/1)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int loss = before - ctx.getPet(0).hp;
            const int dealt = storage_int(ctx, kDamageAudit);
            CHECK(dealt >= 0 && dealt < 300,
                  "T5200.a 1202：本刀伤害 <300 触发门（审计 " +
                      std::to_string(dealt) + "）");
            CHECK_EQ(loss, 0, "T5200.a 1202：点数减伤 300 → 对手 87-102 全扣（实测掉血 " +
                                  std::to_string(loss) + "）");
            CHECK_EQ(storage_int(ctx, kChargeAudit), 0,
                     "T5200.a 1202：1 次额度已消耗（剩余 " +
                         std::to_string(storage_int(ctx, kChargeAudit)) + "）");
        }
        {   // (b) 高伤不触发：本刀 ≥300 → 不授次数 → 对手伤害正常 [87,102]
            ScenarioBattle b(make_party(mk_owner(kBingMai, /*atk=*/300)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int loss = before - ctx.getPet(0).hp;
            const int dealt = storage_int(ctx, kDamageAudit);
            CHECK(dealt >= 300, "T5200.b 1202：本刀伤害 ≥300（审计 " +
                                    std::to_string(dealt) + "）");
            CHECK_EQ(storage_int(ctx, kChargeAudit), -1,
                     "T5200.b 1202：未授减伤次数（键缺席）");
            CHECK(loss >= 87 && loss <= 102,
                  "T5200.b 1202：对手伤害正常落地 [87,102]（实测 " +
                      std::to_string(loss) + "）");
        }
        {   // (c) 对照：不施放载体 → 无次数 → 对手伤害 [87,102]
            ScenarioBattle b(make_party(mk_owner(kPaiDa, /*atk=*/300)),
                             make_party(mk_foe()));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(0).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int loss = before - ctx.getPet(0).hp;
            CHECK_EQ(storage_int(ctx, kChargeAudit), -1,
                     "T5200.c 对照：无减伤次数");
            CHECK(loss >= 87 && loss <= 102,
                  "T5200.c 对照：对手伤害 [87,102]（实测 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T5200] 1202 低伤门 → 下 N 次点数减伤 通过\n";
    }

    std::cout << "scenario_5200 PASS (J197 低伤→下N次点减伤族)" << std::endl;
    return 0;
}
