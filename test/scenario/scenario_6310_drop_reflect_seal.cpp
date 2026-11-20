// 场景 6310 —— J298 下降双倍反馈+封技族（1289，moves_lib/j_drop_reflect_seal.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6310-6319）
//
// 断言口径（逐槽快照 + 绝对量，不看等级和）：
//   - 1289（36720 炽风雷灼，纯链 [1289]|[1]）：自身负槽 -2/-1 → 对手同槽 -4/-2
//     （双倍镜像）；自身下降**不清除**。
//   - 反馈成功 → 对手下 1 次技能无效（次数型 SEAL_ALL）：R2 对手攻击被封（己方
//     hp 不变），R3 第二次攻击恢复（hp 下降）——次数型"响应即消耗"。
//   - 反馈未落地两档：①对手免弱（STAT_DROP 免疫票）②自身无负槽 → 不授封技票。
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

constexpr int kCarrier1289 = 36720;   // 炽风雷灼 [1289]|[1] 特130（纯链）
constexpr int kPaiDa = 10004;         // 拍打：物 40

constexpr int kLandedKey = 991289100;   // +owner
constexpr int kSealKey = 991289200;     // +owner

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier1289, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe() {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/300, /*hp=*/20000);
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

void set_level(BattleContext& ctx, int side, int slot, int level) {
    ctx.ability_levels[side][slot] = level;
    ctx.ws.view_levels[side][slot] = level;
}

void self_round(ScenarioBattle& b, BattleFsm::ActionType action, int arg) {
    b.submit_operation(0, action, arg);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

void foe_round(ScenarioBattle& b, BattleFsm::ActionType action, int arg) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, action, arg);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6310.a 双倍反馈 + 封技 1 次 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 0, 0, -2);   // 物攻 -2
        set_level(ctx, 0, 1, -1);   // 特攻 -1
        self_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 1289
        CHECK_EQ(stor(ctx, kLandedKey + 0), 2, "T6310.a 反馈落地 2 槽");
        CHECK_EQ(ctx.ability_levels[1][0], -4, "T6310.a 对手物攻 -2×2 = -4");
        CHECK_EQ(ctx.ability_levels[1][1], -2, "T6310.a 对手特攻 -1×2 = -2");
        CHECK_EQ(ctx.ability_levels[0][0], -2, "T6310.a 自身下降不清除（物攻仍 -2）");
        CHECK_EQ(ctx.ability_levels[0][1], -1, "T6310.a 自身下降不清除（特攻仍 -1）");
        CHECK_EQ(stor(ctx, kSealKey + 0), 1, "T6310.a 授予封技 1 次");

        const int hp_before = ctx.getPet(0).hp;
        foe_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);    // R2 第一次攻击被封
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "T6310.a R2 攻击被封（hp 不变）");

        foe_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);    // R3 第二次攻击恢复
        const int lost = hp_before - ctx.getPet(0).hp;
        CHECK(lost > 0, "T6310.a R3 次数型用尽 → 正常命中（实测掉血 " +
                            std::to_string(lost) + "）");
    }

    // ══ T6310.b 对手免弱 → 反馈未落地 → 不授封技 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        set_level(ctx, 0, 0, -2);
        ctx.grant_immunity(/*owner=*/1, ImmunityType::STAT_DROP, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/6310, /*soul_immunity=*/false);
        self_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor(ctx, kLandedKey + 0), 0, "T6310.b 免弱挡下 → 落地 0 槽");
        CHECK_EQ(stor(ctx, kSealKey + 0), 0, "T6310.b 不授封技");
        CHECK_EQ(ctx.ability_levels[1][0], 0, "T6310.b 对手物攻仍 0");

        const int hp_before = ctx.getPet(0).hp;
        foe_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);    // R2 攻击照常
        CHECK(hp_before - ctx.getPet(0).hp > 0,
              "T6310.b 无封技 → R2 攻击正常命中");
    }

    // ══ T6310.c 自身无负槽 → 反馈未落地 → 不授封技 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe()));
        b.start();
        BattleContext& ctx = b.ctx();
        self_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK_EQ(stor(ctx, kLandedKey + 0), 0, "T6310.c 无负槽 → 落地 0");
        CHECK_EQ(stor(ctx, kSealKey + 0), 0, "T6310.c 不授封技");
        const int hp_before = ctx.getPet(0).hp;
        foe_round(b, BattleFsm::ActionType::SELECT_SKILL, 0);
        CHECK(hp_before - ctx.getPet(0).hp > 0,
              "T6310.c 无封技 → R2 攻击正常命中");
    }

    std::cout << "scenario 6310 OK" << std::endl;
    return 0;
}
