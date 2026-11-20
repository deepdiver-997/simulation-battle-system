// 场景 5340 —— J208 先手吸取+当回合封攻族（1867，moves_lib/j_first_drain_seal_attack.cpp）
//（2026-10-22，号段 5340-5339）
//
// 断言口径：
//   - 1867（极涛裂空杀 36689 链 [1810,1867,2033]|[4,2]，1867 段 [4]，物 150）：
//     · (a) 先手：吸取对手 max/4=24999，自身 hp 1000→25999；对手本回合物理攻击
//       [87,102] 被 DAMAGE 免伤票全挡 → 自身 hp 仍 25999；对手总掉血 = 主伤+24999。
//     · (b) 后手：条件不成立 → 无吸取/无免伤；对手先手攻击正常落地，自身不回血，
//       对手仅掉主伤 [1,3]；审计缺席（绝对值断言）。
//     · (c) 先手 + 对手特殊攻击 → 同样被"无法造成攻击伤害"挡下。
//   - 审计槽 990186700+0 = 吸取量 24999。
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

constexpr int kPaiDa   = 10004;  // 拍打：物 40
constexpr int kShuiQiang = 10013; // 水枪：特 40
constexpr int kJiTao   = 36689;  // [1810,1867,2033]|[4,2] → 1867 段 [4]
constexpr int kAuditKey = 990186700;
constexpr int kOwnerMaxHp = 100000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int speed) {
    ElfPet p = make_pet(1, "己方", {kJiTao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        speed, /*atk=*/1, /*hp=*/kOwnerMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int speed, int attack_skill) {
    ElfPet p = make_pet(2, "对手", {attack_skill, attack_skill, attack_skill, attack_skill,
                                    attack_skill},
                        speed, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

int audit(BattleContext& ctx) {
    const auto it = ctx.plugin_storage.find(kAuditKey);
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

    // ══ T5340 1867 先手吸取 + 当回合封攻 ══
    {
        {   // (a) 先手 + 对手物理攻击
            ScenarioBattle b(make_party(mk_owner(/*speed=*/300)),
                             make_party(mk_foe(/*speed=*/100, kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;   // 半血以下，回血不被 max 钳
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int foe_loss = foe_before - ctx.getPet(1).hp;
            CHECK_EQ(audit(ctx), 24999, "T5340.a 1867：吸取量 max/4=24999（审计 " +
                                            std::to_string(audit(ctx)) + "）");
            CHECK_EQ(ctx.getPet(0).hp, 1000 + 24999,
                     "T5340.a 1867：自身回血 24999 且对手攻击被免（实测 hp " +
                         std::to_string(ctx.getPet(0).hp) + "）");
            CHECK(foe_loss >= 24999 && foe_loss <= 25005,
                  "T5340.a 1867：对手掉血 = 主伤 + 24999（实测 " +
                      std::to_string(foe_loss) + "）");
        }
        {   // (b) 后手：条件不成立
            ScenarioBattle b(make_party(mk_owner(/*speed=*/100)),
                             make_party(mk_foe(/*speed=*/300, kPaiDa)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;
            const int foe_before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            const int foe_loss = foe_before - ctx.getPet(1).hp;
            const int owner_loss = 1000 - ctx.getPet(0).hp;
            CHECK_EQ(audit(ctx), -1, "T5340.b 1867 对照：后手 → 审计缺席");
            CHECK(foe_loss >= 1 && foe_loss <= 3,
                  "T5340.b 1867 对照：对手仅主伤 [1,3]（实测 " +
                      std::to_string(foe_loss) + "）");
            CHECK(owner_loss >= 87 && owner_loss <= 102,
                  "T5340.b 1867 对照：无免伤 → 对手先手伤害 [87,102]（实测 " +
                      std::to_string(owner_loss) + "）");
        }
        {   // (c) 先手 + 对手特殊攻击亦被封
            ScenarioBattle b(make_party(mk_owner(/*speed=*/300)),
                             make_party(mk_foe(/*speed=*/100, kShuiQiang)));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 1000;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            CHECK_EQ(ctx.getPet(0).hp, 1000 + 24999,
                     "T5340.c 1867：特殊攻击同样无法造成攻击伤害（实测 hp " +
                         std::to_string(ctx.getPet(0).hp) + "）");
        }
        std::cout << "  [T5340] 1867 先手吸取+封攻 + 后手对照 + 特殊攻击封禁 通过\n";
    }

    std::cout << "scenario_5340 PASS (J208 先手吸取+当回合封攻族)" << std::endl;
    return 0;
}
