// 场景 5150 —— J193 击败自身异常族（1933，moves_lib/j_defeat_self_anomaly.cpp）
//（2026-10-22，worktree ../agents/j193；号段 5150-5159）
//
// 断言口径：
//   - 1933（星光·泰山压顶 37665 链 [2220,1395,1933]|[100,7]，物 300）：
//     自身 atk=3000 击杀 50 血对手 → 100% 令**自身**疲惫(7)；审计槽精确 == 7。
//   - 对照：对手 99999 血不被击败 → 审计槽缺席、自身无疲惫（绝对量断言）。
//   - 击杀归因过滤：EVENT_DEATH 的 actor 必须==自身；对手被别的来源击杀不触发
//     （本场景用"未击败"分支覆盖门闭）。
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

constexpr int kPaiDa   = 10004;   // 拍打：物 40（填充刀）
constexpr int kTaiShan = 37665;   // [2220,1395,1933]|[100,7]（击败 → 自身疲惫 100%）
constexpr int kFatigue = 7;       // 疲惫（AbnormalStatusId::Fatigue）
constexpr int kAuditKey = 990193300;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_attacker() {
    ElfPet p = make_pet(1, "己方", {kTaiShan, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/3000, /*hp=*/5000);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/hp);
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

    // ══ T5150 1933 击败对手 → 100% 令自身疲惫 ══
    {
        {   // (a) 击杀 50 血对手 → 自身疲惫 7
            ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe(/*hp=*/50)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(audit(ctx), kFatigue,
                     "T5150.a 1933：击杀后 watcher 触发（审计槽 = 7，实测 " +
                         std::to_string(audit(ctx)) + "）");
            CHECK(ctx.has_active_abnormal_status(0, kFatigue),
                  "T5150.a 1933：击败对手 → 100% 令自身疲惫");
        }
        {   // (b) 对照：对手 99999 血未死 → 不触发、自身无疲惫
            ScenarioBattle b(make_party(mk_attacker()), make_party(mk_foe(/*hp=*/99999)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK_EQ(audit(ctx), -1, "T5150.b 1933 对照：未击败 → watcher 不触发");
            CHECK(!ctx.has_active_abnormal_status(0, kFatigue),
                  "T5150.b 1933 对照：未击败 → 自身无疲惫");
        }
        std::cout << "  [T5150] 1933 击败自身异常 + 未击败对照 通过\n";
    }

    std::cout << "scenario_5150 PASS (J193 击败自身异常族)" << std::endl;
    return 0;
}
