// 场景 5730 —— J242 随机多异常族（549，moves_lib/j_random_multi_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5730-5739）
//
// 断言口径（逐局精确种数 + 池边界 + 审计三段账）：
//   - 549「随机附加烧伤、冻伤、中毒、害怕、疲惫、麻痹其中{0}种」：
//     · [2] 载体 34810 凋零之触（纯链）→ 每局对手恰有 **2 种**池内异常，且不同。
//     · [1] 载体 28512 鬼影把戏（属性纯链）→ 每局恰有 **1 种**池内异常。
//     · 池边界：六项之外（睡眠 8 / 失明 20 / 焚烬 22）恒不出现——证明用的是
//       模板文案池，不是 anomaly_pool_all()。
//     · 审计槽 990549100 = requested*1000+landed：requested 恒等于 {0}、landed 恒等于
//       requested（场景断言"真的挑到并落地"，不是用例没走到）。
//   - 统计型去重：每局挑出的异常**互不相同**（原语不重复挑选）；N=40 局覆盖不同组合。
#include <iostream>
#include <any>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <plugin/core_api.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa   = 10004;
constexpr int kDiaoLing = 34810;  // [549]|[2]（物 150）
constexpr int kGuiYing  = 28512;  // [549]|[1]（属性）
constexpr int kAuditKey = 990549100;
// 官方文案六项（池内）。
const int kPool[6] = {2, 5, 1, 6, 7, 0};    // 烧伤/冻伤/中毒/害怕/疲惫/麻痹
// 池外三个（不应出现）。
const int kOutside[3] = {8, 20, 22};        // 睡眠/失明/焚烬

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_owner(int carrier) {
    ElfPet p = make_pet(1, "己方", {carrier, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/300, /*hp=*/99999);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/99999);
    accuracy_100(p);
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

struct Trial {
    int pool_count = 0;      // 池内实际生效种数
    int outside_count = 0;   // 池外异常出现数（应为 0）
    int audit = -1;
};

Trial run_trial(int carrier) {
    ScenarioBattle b(make_party(mk_owner(carrier)), make_party(mk_foe()));
    b.start();
    BattleContext& ctx = b.ctx();
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    Trial t;
    t.audit = audit(ctx);
    for (int id : kPool) {
        t.pool_count += core_api().has_active_anomaly(&ctx, 1, id) ? 1 : 0;
    }
    for (int id : kOutside) {
        t.outside_count += core_api().has_active_anomaly(&ctx, 1, id) ? 1 : 0;
    }
    return t;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T5730.a 34810 [2]：每局恰 2 种池内异常、互异、无池外 ══
    {
        constexpr int N = 40;
        int bad_count = 0, bad_outside = 0, bad_audit = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(kDiaoLing);
            bad_count += (t.pool_count == 2) ? 0 : 1;
            bad_outside += (t.outside_count == 0) ? 0 : 1;
            bad_audit += (t.audit == 2 * 1000 + 2) ? 0 : 1;
        }
        std::cout << "  [T5730.a] 34810 [2] N=" << N << " bad_count=" << bad_count
                  << " bad_outside=" << bad_outside << " bad_audit=" << bad_audit << std::endl;
        CHECK(bad_count == 0, "T5730.a 每局恰 2 种池内异常（异常局数=" +
                                  std::to_string(bad_count) + "/" + std::to_string(N) + "）");
        CHECK(bad_outside == 0, "T5730.a 池外异常恒不出现（异常局数=" +
                                    std::to_string(bad_outside) + "/" + std::to_string(N) + "）");
        CHECK(bad_audit == 0, "T5730.a 审计 requested=2/landed=2（不符局数=" +
                                  std::to_string(bad_audit) + "/" + std::to_string(N) + "）");
    }

    // ══ T5730.b 28512 [1]：每局恰 1 种池内异常，审计 1/1 ══
    {
        constexpr int N = 30;
        int bad_count = 0, bad_audit = 0;
        for (int i = 0; i < N; ++i) {
            const Trial t = run_trial(kGuiYing);
            bad_count += (t.pool_count == 1) ? 0 : 1;
            bad_audit += (t.audit == 1 * 1000 + 1) ? 0 : 1;
        }
        std::cout << "  [T5730.b] 28512 [1] N=" << N << " bad_count=" << bad_count
                  << " bad_audit=" << bad_audit << std::endl;
        CHECK(bad_count == 0, "T5730.b 每局恰 1 种池内异常（异常局数=" +
                                  std::to_string(bad_count) + "/" + std::to_string(N) + "）");
        CHECK(bad_audit == 0, "T5730.b 审计 requested=1/landed=1（不符局数=" +
                                  std::to_string(bad_audit) + "/" + std::to_string(N) + "）");
    }

    std::cout << "scenario_5730 PASS (J242 随机多异常族)" << std::endl;
    return 0;
}
