// 场景 5320 —— J206 双概率害怕族（460，moves_lib/j_double_chance_fear.cpp）
//（2026-10-22，号段 5320-5319）
//
// 断言口径：
//   - 460（旋风破 13924 纯链 [460]|[40,20]，物 80）：
//     · (a) 对手无提升态 → 申报概率 40；24 局内 rolled 与害怕落地自洽，且至少一次落地。
//     · (b) 对手攻击 +1（提升态）→ 申报概率 40+20=60；24 局同样自洽且至少一次落地。
//   - 审计槽 990460100+0 = rolled*1000 + p；场景按审计分支断言（不猜随机结果）。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;
constexpr int kXuanFeng = 13924;  // [460]|[40,20]
constexpr int kFearKey = static_cast<int>(AbnormalStatusId::Fear);
constexpr int kAuditKey = 990460100;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_owner() {
    ElfPet p = make_pet(1, "己方", {kXuanFeng, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
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

// 返回 landed 次数；每局断言申报概率 == expected_pct。
int run_branch(int trials, bool foe_boost, int expected_pct, const char* tag) {
    int landed = 0;
    for (int t = 0; t < trials; ++t) {
        ScenarioBattle b(make_party(mk_owner()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        if (foe_boost) {
            (void)stat_change(&ctx, /*target=*/1, /*stat=*/0, /*delta=*/1);
        }
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int a = audit(ctx);
        CHECK(a >= 0, std::string(tag) + " 460：审计存在（trial " + std::to_string(t) + "）");
        const bool rolled = (a / 1000) == 1;
        const int pct = a % 1000;
        CHECK_EQ(pct, expected_pct,
                 std::string(tag) + " 460：申报概率 " + std::to_string(expected_pct) +
                     "（实测 " + std::to_string(pct) + "）");
        const bool has_fear = ctx.has_active_abnormal_status(1, kFearKey);
        if (rolled) {
            CHECK(has_fear, std::string(tag) + " 460：rolled=1 → 害怕落地");
            ++landed;
        } else {
            CHECK(!has_fear, std::string(tag) + " 460：rolled=0 → 无害怕");
        }
    }
    return landed;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5320 460 双概率害怕 ══
    {
        const int landed_normal = run_branch(24, /*foe_boost=*/false, 40, "T5320.a");
        CHECK(landed_normal > 0, "T5320.a 460：24 局（p=40）至少一次落地");
        const int landed_boost = run_branch(24, /*foe_boost=*/true, 60, "T5320.b");
        CHECK(landed_boost > 0, "T5320.b 460：24 局（p=60）至少一次落地");
        std::cout << "  [T5320] 460 双概率害怕（p40 landed " << landed_normal
                  << "/24；p60 landed " << landed_boost << "/24）通过\n";
    }

    std::cout << "scenario_5320 PASS (J206 双概率害怕族)" << std::endl;
    return 0;
}
