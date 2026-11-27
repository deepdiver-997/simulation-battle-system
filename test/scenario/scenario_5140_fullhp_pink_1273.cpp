// 场景 5140 —— J192 满体力比例粉伤族（1273，moves_lib/j_fullhp_pink_1273.cpp）
//（2026-10-22，worktree ../agents/j192；号段 5140-5149）
//
// 断言口径：
//   - 1273（魔枝肃杀 34956 纯链 [1273]|[3]，物 140）：自身满体力 5000/5000 时，
//     粉伤 = 5000/3 = 1666（审计槽精确断言），对手掉血带 = 1666 + 主伤[1,2]。
//   - 对照：自身压到 4000/5000（非满）→ 门闭、审计槽缺席，对手只吃主伤 [1,2]。
//   - 主伤用 atk=1（实测 1-2 点），相邻档差 1666 远大于波动；门禁用绝对量断言。
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

constexpr int kPaiDa  = 10004;  // 拍打：物 40（填充刀）
constexpr int kMoZhi   = 34956;  // [1273]|[3]（满血粉伤 max/3 = 1666）
constexpr int kAuditKey = 990127300;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_attacker() {
    ElfPet p = make_pet(1, "己方", {kMoZhi, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

int audit(const BattleContext& ctx) {
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

    // 两局原本连续消费同一全局随机序列；显式接力以保持对照局的伤害浮动。
    std::uint64_t rng_state = 1;
    // ══ T5140 1273 满体力 → 附加 max_hp/{0} 百分比伤害 ══
    {
        {   // (a) 满体力：审计槽 1666，对手掉血 1666 + 主伤[1,2]
            ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe()));
            b.ctx().seed_rng(static_cast<unsigned>(rng_state));
            b.start();
            BattleContext& ctx = b.ctx();
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            rng_state = ctx.rng().state();
            CHECK_EQ(audit(ctx), 1666,
                     "T5140.a 1273：满体力粉伤审计 = max/3 = 1666（实测 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(loss >= 1667 && loss <= 1669,
                  "T5140.a 1273：对手掉血 = 1666 + 主伤[1,2]（实测 " +
                      std::to_string(loss) + "）");
        }
        {   // (b) 对照：非满体力 → 门闭，无审计槽、只吃主伤
            ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe()));
            b.ctx().seed_rng(static_cast<unsigned>(rng_state));
            b.start();
            BattleContext& ctx = b.ctx();
            ctx.getPet(0).hp = 4000;   // 非满体力
            const int before = ctx.getPet(1).hp;
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int loss = before - ctx.getPet(1).hp;
            CHECK_EQ(audit(ctx), -1, "T5140.b 1273 对照：非满体力 → 不写审计槽");
            CHECK(loss >= 1 && loss <= 2,
                  "T5140.b 1273 对照：只吃主伤[1,2]（实测 " + std::to_string(loss) + "）");
        }
        std::cout << "  [T5140] 1273 满体力粉伤 + 非满对照 通过\n";
    }

    std::cout << "scenario_5140 PASS (J192 满体力比例粉伤族)" << std::endl;
    return 0;
}
