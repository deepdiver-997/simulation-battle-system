// 场景 3740 —— K230 异常护盾/PP归零+受击门恢复族（1562，soul_lib/k230_anomaly_shield_pp.cpp）
//（2026-10-08，夜跑 k-prod-6；号段 3740-3749 → T3740 一块六例）
//
// 断言口径（审计槽精确断言 + 行为等式双保险）：
//   - a 异常→护盾：R1 前直铺烧伤（apply_anomaly，007 同款）→ 盾 total = max/3 精确；
//     对手 PP 不动（分支快照=挂盾，无 PP 归零）；门值审计 = max/3。
//   - b 无异常→PP归零：恰 1 槽归零（db[1]=1 消费证据；pp_zero_slot 专用原语），
//     盾 total = 0。
//   - c 受击门正例+回复等式（row1）：小伤（~65 < 门 33333）→ hp 等式
//     hp0 - 实扣审计 + max/3（db[3]=3 消费）；门值审计 = 33333（db[2] 消费）。
//   - d 受击门负例：大伤（atk=250000 → 实扣 ≥44000 > 门，k 值域 0.207~0.33 实测保证）
//     → 不回复：hp 等式 hp0 - 实扣。
//   - e row2 门值 = max/2 精确（db[2]=2 消费证据，行间互锚）；行为一致性：
//     实扣 <49999 ⟺ 回复。
//   - f 分支每回合重估：R1 无异常→PP 归零 1 槽；R1 后铺烧伤 → R2 盾 total = max/3
//     且对手仍只有 1 槽归零（快照分支，不重执行）。
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaDa = 10004;   // 拍打：filler
constexpr int kBurn = static_cast<int>(AbnormalStatusId::Burn);
constexpr int kDmgSumBase = 991425000;   // +owner：受击红伤累计（实现文件同锚）
constexpr int kGateBase = 991426000;     // +owner：受击门值审计

SoulMark make_mark(const std::vector<int>& row_args) {
    SoulMark sm(1562, "K230 魂印", "test", EffectArgs(row_args));
    sm.monster_id = 3962;   // 极岳魔兽
    return sm;
}

ElfPet mark_pet(const std::vector<int>& row_args, int hp) {
    return make_pet(1, "印主", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                    /*speed=*/300, /*atk=*/300, hp, make_mark(row_args));
}

ElfPet foe_pet(int atk) {
    return make_pet(2, "对手", {kPaDa, kPaDa, kPaDa, kPaDa, kPaDa},
                    /*speed=*/100, atk, 99999);
}

int audit_int(BattleContext& ctx, int key) {
    const auto it = ctx.plugin_storage.find(key);
    return it == ctx.plugin_storage.end() ? -1 : std::any_cast<int>(it->second);
}

int zero_pp_slots(BattleContext& ctx) {
    int zero = 0;
    for (const Skills& s : ctx.getPet(1).skills) {
        if (s.maxPP > 0 && s.pp == 0) {
            ++zero;
        }
    }
    return zero;
}

void one_round(ScenarioBattle& b, BattleFsm::ActionType a1) {
    b.submit_operation(0, BattleFsm::ActionType::NONE, -1);
    b.submit_operation(1, a1, a1 == BattleFsm::ActionType::SELECT_SKILL ? 0 : -1);
    b.drive();
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    const std::vector<int> row1{3, 1, 3, 3, 1};
    const std::vector<int> row2{3, 1, 2, 3, 1};

    // ══ T3740.a 异常 → 挂盾 max/3（分支快照=盾，不 PP 归零）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99999)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, /*duration_rounds=*/3, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T3740.a 烧伤直铺成功");
        one_round(b, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 99999 / 3, "T3740.a 盾 = max/3 精确");
        CHECK_EQ(zero_pp_slots(ctx), 0, "T3740.a 异常分支不归零对手 PP");
        CHECK_EQ(audit_int(ctx, kGateBase + 0), 99999 / 3, "T3740.a 门值审计 = max/3");
        std::cout << "  [T3740.a] 异常→护盾 通过\n";
    }

    // ══ T3740.b 无异常 → 对手随机 1 技能 PP 归零（db[1]=1）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99999)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::NONE);
        CHECK_EQ(zero_pp_slots(ctx), 1, "T3740.b 恰 1 技能 PP 归零");
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 0, "T3740.b 无异常不挂盾");
        std::cout << "  [T3740.b] 无异常→PP归零 通过\n";
    }

    // ══ T3740.c 受击门正例+回复等式（row1：db[2]=3 门、db[3]=3 回）══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99999)), make_party(foe_pet(300)));
        b.start();
        BattleContext& ctx = b.ctx();
        ctx.seerRobot[0].elfPets[0].hp = 66666;   // 预留 1/3 回复余量（满血封顶会吞回复）
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        const int dmg = audit_int(ctx, kDmgSumBase + 0);
        CHECK(dmg >= 1 && dmg < 99999 / 3, "T3740.c 实扣红伤 < 门（正例前提）");
        CHECK_EQ(audit_int(ctx, kGateBase + 0), 99999 / 3, "T3740.c 门值审计 = max/3");
        CHECK_EQ(ctx.getPet(0).hp, 66666 - dmg + 99999 / 3, "T3740.c 回复等式 = max/3");
        std::cout << "  [T3740.c] 受击门正例+回复等式 通过\n";
    }

    // ══ T3740.d 受击门负例：大伤 ≥ 门 → 不回复 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99999)), make_party(foe_pet(250000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        const int dmg = audit_int(ctx, kDmgSumBase + 0);
        CHECK(dmg >= 99999 / 3, "T3740.d 实扣红伤 ≥ 门（负例前提，atk=250000 保底）");
        CHECK_EQ(ctx.getPet(0).hp, hp0 - dmg, "T3740.d 伤过门不回复");
        std::cout << "  [T3740.d] 受击门负例 通过\n";
    }

    // ══ T3740.e row2 门值 = max/2（db[2]=2 行间互锚）+ 行为一致性 ══
    {
        ScenarioBattle b(make_party(mark_pet(row2, 99999)), make_party(foe_pet(250000)));
        b.start();
        BattleContext& ctx = b.ctx();
        const int hp0 = ctx.getPet(0).hp;
        one_round(b, BattleFsm::ActionType::SELECT_SKILL);
        const int dmg = audit_int(ctx, kDmgSumBase + 0);
        CHECK_EQ(audit_int(ctx, kGateBase + 0), 99999 / 2, "T3740.e row2 门值审计 = max/2");
        if (dmg < 99999 / 2) {
            CHECK_EQ(ctx.getPet(0).hp, hp0 - dmg + 99999 / 3, "T3740.e 未过 1/2 门 → 回复");
        } else {
            CHECK_EQ(ctx.getPet(0).hp, hp0 - dmg, "T3740.e 过 1/2 门 → 不回复");
        }
        std::cout << "  [T3740.e] row2 门值+一致性 通过（实扣=" << dmg << "）\n";
    }

    // ══ T3740.f 分支每回合重估：R1 PP 归零 → R2 烧伤转盾、不重执行 ══
    {
        ScenarioBattle b(make_party(mark_pet(row1, 99999)), make_party(foe_pet(1)));
        b.start();
        BattleContext& ctx = b.ctx();
        one_round(b, BattleFsm::ActionType::NONE);
        CHECK_EQ(zero_pp_slots(ctx), 1, "T3740.f R1 无异常 → PP 归零 1 槽");
        CHECK_EQ(apply_anomaly(&ctx, 0, kBurn, /*duration_rounds=*/2, /*actor=*/1),
                 ApplyAnomalyResult::SUCCESS, "T3740.f R1 后铺烧伤");
        one_round(b, BattleFsm::ActionType::NONE);
        CHECK_EQ(ctx.getPet(0).shield_bank_.total(), 99999 / 3, "T3740.f R2 异常 → 盾 max/3");
        CHECK_EQ(zero_pp_slots(ctx), 1, "T3740.f R2 分支转盾，PP 不重归零");
        std::cout << "  [T3740.f] 分支每回合重估 通过\n";
    }

    std::cout << "scenario_3740 PASS (K230 异常护盾/PP归零+受击门恢复族)" << std::endl;
    return 0;
}
