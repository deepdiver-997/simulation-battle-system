// 场景 5270 —— J202 低体回合末比例粉伤族（888，moves_lib/j_low_hp_endturn_pink.cpp）
//（2026-10-22，号段 5270-5279）
//
// 断言口径：
//   - 888（绝地打击 30052 纯链 [888]|[4]，物 130，己方 atk=1）：
//     · (a) 出手时自身 hp=5000 < 对手 99999 → 回合末粉伤 max_hp(opp)/4=24999；
//       对手总掉血 = 主伤 1-2 + 24999 → [24999,25005]；审计槽 990888100+0 == 24999。
//     · (b) 自身 hp=999999 > 对手 99999 → 不触发，仅主伤 [1,3]，审计缺席。
//     · (c) 严格门：双方 hp 相等（99999==99999）→ 不触发（绝对值断言）。
//   - 粉伤是 PERCENT_VALUE 档（走粉伤相），观测对手 hp 前后差。
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

constexpr int kPaiDa    = 10004;  // 拍打：物 40
constexpr int kJueDi    = 30052;  // [888]|[4]（物 130）
constexpr int kAuditKey = 990888100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int hp) {
    ElfPet p = make_pet(1, "己方", {kJueDi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/false);
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

int run_once(int owner_hp, int foe_hp, bool& triggered) {
    ScenarioBattle b(make_party(mk_owner(owner_hp)), make_party(mk_foe(foe_hp)));
    b.start();
    BattleContext& ctx = b.ctx();
    const int before = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    triggered = (audit(ctx) == 24999);
    return before - ctx.getPet(1).hp;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5270 888 低体 → 回合末对手 1/{0} 最大体力粉伤 ══
    {
        {   // (a) 低体触发
            bool triggered = false;
            const int loss = run_once(/*owner_hp=*/5000, /*foe_hp=*/99999, triggered);
            CHECK(triggered, "T5270.a 888：触发（审计 = max_hp/4 = 24999）");
            CHECK(loss >= 24999 && loss <= 25005,
                  "T5270.a 888：对手掉血 = 主伤 + 24999（实测 " +
                      std::to_string(loss) + "）");
        }
        {   // (b) 高体对照
            bool triggered = false;
            const int loss = run_once(/*owner_hp=*/999999, /*foe_hp=*/99999, triggered);
            CHECK(!triggered, "T5270.b 888 对照：自身高于对手 → 不触发（审计缺席）");
            CHECK(loss >= 1 && loss <= 3,
                  "T5270.b 888 对照：仅主伤 [1,3]（实测 " + std::to_string(loss) + "）");
        }
        {   // (c) 严格门：hp 相等不触发
            bool triggered = false;
            const int loss = run_once(/*owner_hp=*/99999, /*foe_hp=*/99999, triggered);
            CHECK(!triggered, "T5270.c 888：hp 相等 → 严格门不触发");
            CHECK(loss >= 1 && loss <= 3,
                  "T5270.c 888：相等时仅主伤（实测 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T5270] 888 低体回合末比例粉伤 + 高体/相等对照 通过\n";
    }

    std::cout << "scenario_5270 PASS (J202 低体回合末比例粉伤族)" << std::endl;
    return 0;
}
