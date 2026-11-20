// 场景 1260 —— J41 后手先制/免致命/miss惩罚族（3 宏，moves_lib/j41_second_preempt.cpp）
//（2026-10-02，worktree ../agents/j41；号段 1260-1269 → T1260-T1262 三块）
//
// 断言口径：
//   - 1549 用双局对照（自身速度 100 vs 对手 300）：R1 让对手先手（自身用 carriers
//     时机=后手）→ R2 preemptive_level[0]==1；先手局（速度 300）对照不写；
//   - 662 用 force_hp_to_zero 原语直呼（窗内 hp 不动、R4 窗过期后归零）；
//   - 648 用 dodge_rate 直写造必然 miss（1235 同款）→ 对手吃冻伤（{0}=5）；
//   - 载体链排查：28227/23543/16542 均单效果载体。
#include <iostream>

#include <entities/pet_factory.h>
#include <entities/numerical-properties.h>
#include <fsm/battleContext.h>
#include <fsm/battleFsm.h>
#include <primitives/battle_primitives.h>
#include <abnormal-system/abnormal-types.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kPaiDa = 10004;   // 拍打：物40
constexpr int kRuQiao = 28227;  // 入鞘[1549]|[1]：属（后手→下回合攻击技先制+1）
constexpr int kLongYu = 23543;  // 不坏龙玉[662]|[3]：属（3回合免致命）
constexpr int kJingHun = 16542; // 惊魂飞天断[648]|[5]：物?（miss→对手冻伤）
constexpr int kFrostbite = static_cast<int>(AbnormalStatusId::Frostbite);
constexpr int kMaxHp = 999999;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet self_pet(std::array<int, 5> skills, int speed = 300, int atk = 1) {
    ElfPet p = make_pet(1, "己方", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet foe_pet(std::array<int, 5> skills, int speed = 100, int atk = 1) {
    ElfPet p = make_pet(2, "对手", skills, speed, atk, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1260 1549 后手→下回合攻击先制+1（入鞘 [1549]|[1]）══
    {
        // a) 后手局：自身速度 100（后手用入鞘）→ R2 preemptive_level +1
        {
            ScenarioBattle b(
                make_party(self_pet({kRuQiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/100)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/300)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 后手用
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);   // R2 拍打
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.ws.preemptive_level[0] >= 1,
                  "T1260.a 1549：后手 → R2 攻击先制 +1（level=" +
                      std::to_string(ctx.ws.preemptive_level[0]) + "）");
        }
        // b) 对照：先手局（速度 300）→ 不武装
        {
            ScenarioBattle b(
                make_party(self_pet({kRuQiao, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                    /*speed=*/300)),
                make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa},
                                   /*speed=*/100)));
            b.start();
            BattleContext& ctx = b.ctx();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
            CHECK(ctx.ws.preemptive_level[0] == 0, "T1260.b 1549 门：先手不触发");
        }
        std::cout << "  [T1260] 1549 后手先制 通过\n";
    }

    // ══ T1261 662 免致命窗（不坏龙玉 [662]|[3]）══
    {
        ScenarioBattle b(make_party(self_pet({kLongYu, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
                         make_party(foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        const int hp_before = ctx.getPet(0).hp;
        (void)force_hp_to_zero(&ctx, 0, 1);   // 窗内秒杀 → 被免疫票挡
        CHECK_EQ(ctx.getPet(0).hp, hp_before, "T1261.a 662：窗内免疫致命一击");
        for (int r = 0; r < 3; ++r) {   // R2/R3/R4 空转过窗
            b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
            b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
            b.drive();
        }
        (void)force_hp_to_zero(&ctx, 0, 1);   // 窗过期 → 归零落地
        CHECK(ctx.getPet(0).hp <= 0, "T1261.b 662：窗过期致命落地");
        std::cout << "  [T1261] 662 免致命窗 通过\n";
    }

    // ══ T1262 648 miss→冻伤（惊魂飞天断 [648]|[5]）══
    {
        ElfPet me = self_pet({kJingHun, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        ElfPet foe = foe_pet({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa});
        for (Skills& s2 : foe.skills) {
            s2.accuracy = 0;   // 必然 miss（accuracy 0 → hit_chance 0；ws 直写会被回合重置）
        }
        ScenarioBattle b(make_party(std::move(me)), make_party(std::move(foe)));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂惩罚
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);   //    对手必 miss
        b.drive();
        CHECK(ctx.has_active_abnormal_status(1, kFrostbite),
              "T1262 648：对手攻击技 miss → 冻伤落地");
        std::cout << "  [T1262] 648 miss惩罚 通过\n";
    }

    std::cout << "scenario_1260 PASS (J41 后手先制/免致命/miss惩罚族 1549/662/648)"
              << std::endl;
    return 0;
}
