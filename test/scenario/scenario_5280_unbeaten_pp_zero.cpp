// 场景 5280 —— J203 未击败随机全PP归零族（739，moves_lib/j_unbeaten_pp_zero.cpp）
//（2026-10-22，号段 5280-5289）
//
// 断言口径：
//   - 739（神话·星河破灭 17166 纯链 [739]|[50]，特 170；己方 atk=1 不会击败对手）：
//     · (a) 16 局独立对局（50% 概率）：审计 = rolled*1000+清零槽数。rolled=1 →
//       对手 5 槽 PP 全 0；rolled=0 → 全 10。断言"至少一次 rolled"（16 局 P(全不中)
//       约 1.5e-5）且每个分支的 PP 与审计自洽——不猜随机结果。
//     · (b) 对照：当回合击败对手（atk=3000 打 50 血）→ 回合末审计缺席（"未击败"门）。
//   - 审计槽 990739100+0；对手 PP 起点固定 10。
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
constexpr int kXingHe   = 17166;  // [739]|[50]（特 170）
constexpr int kMaxPP    = 10;
constexpr int kAuditKey = 990739100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner(int atk) {
    ElfPet p = make_pet(1, "己方", {kXingHe, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, atk, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
    accuracy_100(p, /*must_hit=*/false);
    for (Skills& s : p.skills) {
        s.maxPP = kMaxPP;
        s.pp = kMaxPP;
    }
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

int count_pp(BattleContext& ctx, int side, int value) {
    int n = 0;
    for (const Skills& s : ctx.getPet(side).skills) {
        if (s.pp == value) {
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

    // ══ T5280 739 未击败 → 概率清空对手全技能 PP ══
    {
        int rolled_trials = 0;
        for (int trial = 0; trial < 16; ++trial) {
            ScenarioBattle b(make_party(mk_owner(/*atk=*/1)), make_party(mk_foe(/*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            const int a = audit(ctx);
            CHECK(a >= 0, "T5280.a 739：未击败 → 每局都有审计（trial " +
                              std::to_string(trial) + "）");
            const bool rolled = (a / 1000) == 1;
            const int landed = a % 1000;
            if (rolled) {
                ++rolled_trials;
                CHECK_EQ(count_pp(ctx, 1, 0), 5,
                         "T5280.a 739：rolled=1 → 对手 5 槽 PP 全 0（trial " +
                             std::to_string(trial) + "）");
                CHECK_EQ(landed, 5, "T5280.a 739：审计清零槽数=5");
            } else {
                CHECK_EQ(count_pp(ctx, 1, kMaxPP), 5,
                         "T5280.a 739：rolled=0 → 对手 PP 全 10（trial " +
                             std::to_string(trial) + "）");
                CHECK_EQ(landed, 0, "T5280.a 739：rolled=0 → 审计清零槽数=0");
            }
        }
        CHECK(rolled_trials > 0, "T5280.a 739：16 局中至少一次掷中");
        std::cout << "  [T5280.a] 739 概率清 PP 分支自洽（rolled " +
                         std::to_string(rolled_trials) + "/16）\n";
    }
    {   // (b) 当回合击败 → 不触发
        ScenarioBattle b(make_party(mk_owner(/*atk=*/3000)), make_party(mk_foe(/*hp=*/50)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK_EQ(audit(ctx), -1,
                 "T5280.b 739：当回合击败对手 → 回合末不触发（审计缺席）");
    }

    std::cout << "  [T5280] 739 未击败概率全PP归零 + 击败对照 通过\n";
    std::cout << "scenario_5280 PASS (J203 未击败随机全PP归零族)" << std::endl;
    return 0;
}
