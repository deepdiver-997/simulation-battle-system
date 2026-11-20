// 场景 6350 —— J302 用技双降/粉伤兜底窗族（1393，moves_lib/j_skill_drop_window.cpp）
//（2026-10-22，worktree feat/jbatch；号段 6350-6359）
//
// 断言口径：
//   - 1393（27394 零度压迫，属性技，链 1204/525 不在观测面）：N=3 为 **InRounds** 窗
//     → 生效 [R,R+2]，**施放当回合内联首跳**，R+3 起窗外不再降。
//   - 未触发兜底 = 对手 STAT_DROP 免疫票 → 双降都 IMMUNE → 对手最大体力/{3} 粉伤
//     （9000/3=3000），施放当回合与窗内每回合各结算一次。
//   - AT_FLOOR（已 -6）与 IMMUNE 都算未落地 → 补粉伤（2026-10-25 口径修订）。
//   - 不用技能的回合不结算（NONE → SKILL_EFFECT 桶不跑）。
//   - 逐槽绝对量 + 审计槽 991393100（落地槽数）/991393200（兜底粉伤）/
//     991393300（累计应用次数）。
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

constexpr int kCarrier1393 = 27394;   // 零度压迫 [1204,525,1393]|[1,3,30,3,1,1,3]
constexpr int kPaiDa = 10004;

constexpr int kDefSlot = static_cast<int>(NumericalPropertyIndex::DEFENSE);
constexpr int kSpdSlot = static_cast<int>(NumericalPropertyIndex::SPEED);

constexpr int kLandKey = 991393100;   // +owner
constexpr int kPinkKey = 991393200;   // +owner
constexpr int kApplyKey = 991393300;  // +owner

void accuracy_100(ElfPet& p) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        s.must_hit = 1;
    }
}

ElfPet mk_self() {
    ElfPet p = make_pet(1, "己方", {kCarrier1393, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/300, /*atk=*/1, /*hp=*/5000);
    accuracy_100(p);
    return p;
}

ElfPet mk_foe(int hp) {
    ElfPet p = make_pet(2, "对手", {kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                        /*speed=*/100, /*atk=*/1, hp);
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

void round_self(ScenarioBattle& b, BattleFsm::ActionType action, int arg) {
    b.submit_operation(0, action, arg);
    b.submit_operation(1, BattleFsm::ActionType::NONE, -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T6350.a InRounds 窗 [R1,R3]：R1 首跳、R2/R3 tick，R4 窗外 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 施放（当回合首跳）
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -1, "T6350.a R1 当回合防御 -1");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -1, "T6350.a R1 当回合速度 -1");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 1, "T6350.a R1 应用 1 次");
        CHECK_EQ(stor(ctx, kLandKey + 0), 2, "T6350.a R1 双槽落地");
        CHECK_EQ(stor(ctx, kPinkKey + 0), 0, "T6350.a 落地 → 不兜底");

        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 tick
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -2, "T6350.a R2 防御 -2");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -2, "T6350.a R2 速度 -2");

        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 tick（末回合）
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -3, "T6350.a R3 防御 -3");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -3, "T6350.a R3 速度 -3");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 3, "T6350.a 窗内恰应用 3 次");

        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R4 窗外
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -3, "T6350.a R4 窗外防御不再降");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -3, "T6350.a R4 窗外速度不再降");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 3, "T6350.a R4 无 tick");
    }

    // ══ T6350.b 对手免弱 → 双槽未触发 → 每回合兜底 9000/3=3000 粉伤 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.grant_immunity(/*owner=*/1, ImmunityType::STAT_DROP, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0,
                           /*source_id=*/6350, /*soul_immunity=*/false);

        const int hp_r1 = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳（属性技无红伤）
        CHECK_EQ(stor(ctx, kLandKey + 0), 0, "T6350.b R1 双槽都被免弱 → 落地 0");
        CHECK_EQ(stor(ctx, kPinkKey + 0), 3000, "T6350.b R1 maxHP 9000/3 = 3000 粉伤");
        CHECK_EQ(hp_r1 - ctx.getPet(1).hp, 3000, "T6350.b R1 掉血恰 3000（属性技无红伤）");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], 0, "T6350.b R1 防御未被降");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], 0, "T6350.b R1 速度未被降");

        const int hp_r2 = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 tick（拍打有红伤）
        CHECK_EQ(stor(ctx, kLandKey + 0), 0, "T6350.b R2 落地 0");
        CHECK_EQ(stor(ctx, kPinkKey + 0), 3000, "T6350.b R2 兜底仍 3000");
        const int loss = hp_r2 - ctx.getPet(1).hp;
        CHECK(loss >= 3000 && loss <= 3006,
              "T6350.b R2 掉血 = 粉3000 + 红1~4（链 1204 使下次攻击必暴），实测 "
                  + std::to_string(loss));
        CHECK_EQ(stor(ctx, kApplyKey + 0), 2, "T6350.b R2 应用 2 次");
    }

    // ══ T6350.c 不用技能的回合不结算 ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳
        round_self(b, BattleFsm::ActionType::NONE, -1);         // R2 不出手
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -1, "T6350.c NONE 回合不降防");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 1, "T6350.c NONE 回合不应用");
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R3 出手（末回合 tick）
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -2, "T6350.c R3 出手 → 防御 -2");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 2, "T6350.c 应用 2 次");
    }

    // ══ T6350.d 对手已 -6：全 AT_FLOOR = 全未落地 → 补粉伤（2026-10-25 口径）══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(9000)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.ability_levels[1][kDefSlot] = -6;
        ctx.ability_levels[1][kSpdSlot] = -6;
        const int hp_before = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳
        CHECK_EQ(stor(ctx, kLandKey + 0), 0, "T6350.d 全 AT_FLOOR 计为未落地");
        CHECK_EQ(stor(ctx, kPinkKey + 0), 3000, "T6350.d 全未落地 → 补粉伤（9000/3）");
        CHECK(hp_before - ctx.getPet(1).hp >= 3000, "T6350.d 对手掉血 ≥ 粉伤");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -6, "T6350.d 防御仍 -6（钳制）");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -6, "T6350.d 速度仍 -6（钳制）");
    }

    // ══ T6350.e miss 回合仍算"使用技能"（后续 tick 挂 BEFORE_SKILL_HIT）══
    {
        ElfPet self = mk_self();
        self.skills[1].accuracy = 0;   // 拍打必 miss（且清必中）
        self.skills[1].must_hit = 0;
        ScenarioBattle b(make_party(self), make_party(mk_foe(20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -1, "T6350.e R1 防御 -1");
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 miss
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -2, "T6350.e miss 回合仍降防 -2");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -2, "T6350.e miss 回合仍降速 -2");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 2, "T6350.e miss 回合仍应用");
    }

    // ══ T6350.f 攻击技被盔（SKILL_INVALID）回合仍算"使用技能" ══
    {
        ScenarioBattle b(make_party(mk_self()), make_party(mk_foe(20000)));
        b.start();
        BattleContext& ctx = b.ctx();
        // 对手（1）给己方（0）挂 1 次攻击盔：R2 的拍打无效。
        ctx.grant_skill_invalid(/*source=*/1, /*target=*/0, /*source_slot=*/-1,
                                /*effect_id=*/6350, SealKind::SEAL_ATTACK,
                                /*counts=*/1, /*rounds=*/0, /*penetrable=*/false,
                                EffectScope::ON_STAGE);
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 首跳
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -1, "T6350.f R1 防御 -1");
        const int hp_before = ctx.getPet(1).hp;
        round_self(b, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打被盔
        CHECK_EQ(ctx.getPet(1).hp, hp_before, "T6350.f R2 被盔 → 无红伤");
        CHECK_EQ(ctx.ability_levels[1][kDefSlot], -2, "T6350.f 被盔回合仍降防 -2");
        CHECK_EQ(ctx.ability_levels[1][kSpdSlot], -2, "T6350.f 被盔回合仍降速 -2");
        CHECK_EQ(stor(ctx, kApplyKey + 0), 2, "T6350.f 被盔回合仍应用");
    }

    std::cout << "scenario 6350 OK" << std::endl;
    return 0;
}
