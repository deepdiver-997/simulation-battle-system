// 场景 6320 —— J299 损PP固伤转真族（1668，moves_lib/j_foe_pptrue.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6320-6329）
//
// 断言口径：
//   - 1668（36686 暴雨倾泻 [1114,1668]|[2,25]，1114 在对手无回合效果时静默）：
//     **对手**已损失 PP 总和 × 25 = 固定伤害；种 10+5=15 点损失 → 375 固伤。
//   - 免疫型转真：对手挂 PINK_DAMAGE 免疫票 → final<=0 && absorbed==0 → 额外等量真伤
//     （掉血仍 ≈375，用审计槽 991668102 证明走的是真伤支路）。
//   - 护罩吃光 = absorbed>0 ≠ 免疫 → **不转真**（掉血仅红伤 1~2，护罩 1000→625）。
//   - 零损失档 → 0 值、不结算；审计槽精确断言前提。
#include <iostream>
#include <string>
#include <any>

#include <entities/numerical-properties.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kCarrier1668 = 36686;   // 暴雨倾泻 [1114,1668]|[2,25] 物85
constexpr int kPaiDa = 10004;

constexpr int kLostPpKey = 991668100;   // +owner
constexpr int kFixedKey = 991668101;    // +owner
constexpr int kTrueKey = 991668102;     // +owner

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier1668, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, /*hp=*/20000);
    accuracy_100(p);
    return p;
}

int stor(const BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    if (it == ctx.plugin_storage.end()) {
        return 0;
    }
    const int* p = std::any_cast<int>(&it->second);
    return p ? *p : 0;
}

// 给对手种 10+5 点 PP 损失（槽 1/2；maxPP 不足则钳到 0，场景前置用审计槽复核）。
void seed_foe_pp_loss(BattleContext& ctx) {
    ElfPet& foe = ctx.getPet(1);
    if (foe.skills[1].maxPP >= 10) {
        foe.skills[1].pp = foe.skills[1].maxPP - 10;
    }
    if (foe.skills[2].maxPP >= 5) {
        foe.skills[2].pp = foe.skills[2].maxPP - 5;
    }
}

void self_round(ScenarioBattle& b) {
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6320.a 无免疫：15×25=375 固伤，不转真 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_foe_pp_loss(ctx);
        const int hp_before = ctx.getPet(1).hp;
        self_round(b);
        CHECK_EQ(stor(ctx, kLostPpKey + 0), 15, "T6320.a 对手已损失 PP 总和 15");
        CHECK_EQ(stor(ctx, kFixedKey + 0), 375, "T6320.a 15×25 = 375 固定伤害");
        CHECK_EQ(stor(ctx, kTrueKey + 0), 0, "T6320.a 未免疫 → 不转真");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK(loss >= 375 && loss <= 378,
              "T6320.a 掉血 = 固伤375 + 红伤1~2，实测 " + std::to_string(loss));
    }

    // ══ T6320.b 免疫固定伤害 → 等量真伤 375 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_foe_pp_loss(ctx);
        ctx.grant_immunity(/*owner=*/1, ImmunityType::PINK_DAMAGE, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/6320, /*soul_immunity=*/false);
        const int hp_before = ctx.getPet(1).hp;
        self_round(b);
        CHECK_EQ(stor(ctx, kFixedKey + 0), 375, "T6320.b 固定伤害段仍申报 375");
        CHECK_EQ(stor(ctx, kTrueKey + 0), 375, "T6320.b 被免疫 → 等量真伤 375");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK(loss >= 375 && loss <= 378,
              "T6320.b 掉血 = 真伤375 + 红伤1~2，实测 " + std::to_string(loss));
    }

    // ══ T6320.c 护罩吸收（absorbed>0）→ 不转真 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        seed_foe_pp_loss(ctx);
        ctx.getPet(1).hood_bank_.add(/*priority=*/0, 1000, /*source_id=*/63201,
                                     /*duration=*/0);
        const int hp_before = ctx.getPet(1).hp;
        self_round(b);
        CHECK_EQ(stor(ctx, kTrueKey + 0), 0, "T6320.c 护罩吸收 ≠ 免疫 → 不转真");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK(loss <= 5, "T6320.c 掉血仅红伤 1~2（实测 " + std::to_string(loss) + "）");
        const int hood = ctx.getPet(1).hood_bank_.total();
        CHECK(hood >= 620 && hood <= 630,
              "T6320.c 护罩 1000−375 = 625（实测 " + std::to_string(hood) + "）");
    }

    // ══ T6320.d 零损失档：0 值、不结算 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp_before = ctx.getPet(1).hp;
        self_round(b);
        CHECK_EQ(stor(ctx, kLostPpKey + 0), 0, "T6320.d 全额 PP → 损失 0");
        CHECK_EQ(stor(ctx, kFixedKey + 0), 0, "T6320.d 固定伤害 0");
        CHECK_EQ(stor(ctx, kTrueKey + 0), 0, "T6320.d 不转真");
        const int loss = hp_before - ctx.getPet(1).hp;
        CHECK(loss <= 5, "T6320.d 仅红伤 1~2（实测 " + std::to_string(loss) + "）");
    }

    std::cout << "scenario 6320 OK" << std::endl;
    return 0;
}
