// 场景 5970 —— J266 能力下降随机异常族（1492，moves_lib/j_drop_rand_anomaly.cpp）
//（2026-10-22，worktree feat/jbatch；号段 5970-5979 → T5970.a-d 四块）
//
// 断言口径：
//   - 载体 金坪镇华殿 38058 [1639,1492]|[1]：1492 种数=1；1639 需自身有正等级才必暴
//     （本场景自身六槽 0 → 惰性）。
//   - 门开 = 对手六槽任一负等级 → 随机附加 1 种异常（审计 landed=1 + 当前异常种数=1）。
//   - 门闭 = 全 0 / 仅正等级 / **自身**负等级（对手干净）→ landed=0、异常种数=0。
//   - 异常种数用 ctx.has_active_abnormal_status 扫 0..43 统计（不依赖具体随机种）。
#include <any>
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kJinPing = 38058;   // 金坪镇华殿：特 [1639,1492]|[1]
constexpr int kPaiDa   = 10004;   // 拍打：filler
constexpr int kMaxHp   = 99999;

constexpr int kAuditBase = 991492200;   // +owner：landed 种数

int storage_int(BattleContext& ctx, int key, int fallback) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return fallback;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : fallback;
}

int active_anomaly_count(BattleContext& ctx, int side) {
    int n = 0;
    for (int id = 0; id <= 43; ++id) {
        if (ctx.has_active_abnormal_status(side, id)) {
            ++n;
        }
    }
    return n;
}

void always_hit(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
}

ElfPet self_pet() {
    ElfPet p = make_pet(1, "己方", {kJinPing, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1000, kMaxHp);
    always_hit(p);
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, kMaxHp);
    always_hit(p);
    return p;
}

void self_skill(ScenarioBattle& b) {
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

    // ══ T5970.a 门开：对手防御 -1 → 随机附加 1 种异常 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][2] = -1;
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditBase + 0, -1), 1,
                 "T5970.a 1492：门开 landed=1");
        CHECK_EQ(active_anomaly_count(ctx, 1), 1,
                 "T5970.a 1492：对手当前异常种数=1");
        std::cout << "  [T5970.a] 1492 门开 通过\n";
    }

    // ══ T5970.b 门闭（全 0）：不附加异常 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditBase + 0, -1), 0, "T5970.b 1492：门闭 landed=0");
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T5970.b 1492：无异常落地");
        std::cout << "  [T5970.b] 1492 门闭（全 0） 通过\n";
    }

    // ══ T5970.c 门闭（仅正等级）：+1 不是能力下降 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        for (int i = 0; i < BattleContext::kAbilityLevelSlotCount; ++i) {
            ctx.ability_levels[1][i] = 1;
        }
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditBase + 0, -1), 0,
                 "T5970.c 1492：仅正等级 → 门闭");
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T5970.c 1492：无异常落地");
        std::cout << "  [T5970.c] 1492 门闭（仅正等级） 通过\n";
    }

    // ══ T5970.d 门读对手而非自身：自身 -1、对手干净 → 门闭 ══
    {
        ScenarioBattle b(make_party(self_pet()), make_party(foe_pet()));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[0][0] = -1;
        self_skill(b);
        CHECK_EQ(storage_int(ctx, kAuditBase + 0, -1), 0,
                 "T5970.d 1492：自身下降不计入门");
        CHECK_EQ(active_anomaly_count(ctx, 1), 0, "T5970.d 1492：无异常落地");
        std::cout << "  [T5970.d] 1492 门侧位 通过\n";
    }

    std::cout << "scenario_5970 PASS (J266 能力下降随机异常族)" << std::endl;
    return 0;
}
