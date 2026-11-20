// 场景 1700 —— J80 弹控变体族（2 宏，moves_lib/j_reflect_variants.cpp）
//（2026-10-03，worktree ../agents/j80；号段 1700-1709 → T1700-T1701 两块）
//
// 断言口径（mask 位语义：置位=免疫该 id，消费端 rule_center.h:553；reflect 只在
// 免疫挡下后触发 → mask 同管两半）：
//   - 1932 [4,排5]：施加冰封(5)（排除码）→照常落在自身；施加中毒(1)→被挡+弹回；
//     R5 窗过期（[4]→R1..R4）→中毒可落地；
//   - 1687 [3]：施加麻痹(0)（控制类）→被挡+弹回；施加中毒(1)（非控制）→落在自身；
//   - 场景内施加走 apply_anomaly（Modern 通道，actor=对手 → 反弹落在对手身上）；
//   - 载体：水晶之匿 [1783✗,1932,1039]|[5,2]（1783 哑）/ 灵威神谕 [1687,878✗]|[3,3]
//     （878 哑，近纯链）。
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

constexpr int kPaiDa  = 10004;  // 拍打：物40
constexpr int kShuiJing = 28249; // 水晶之匿 [1783,1932,1039]|[5,2,4,5,…]：属
                                 //（1932 切片=[4,5]：4 回合、排除冰封(5)；1783 哑）
constexpr int kLingWei  = 36555; // 灵威神谕 [1687,878]|[3,3]：物（控制类弹控载体）
constexpr int kMaxHp  = 5000;

void accuracy_100(ElfPet& p, bool must_hit) {
    for (Skills& s : p.skills) {
        s.accuracy = 100;
        if (must_hit) {
            s.must_hit = 1;
        }
    }
}

ElfPet mk_self(std::array<int, 5> skills) {
    ElfPet p = make_pet(1, "己方", skills, 300, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/true);
    return p;
}

ElfPet mk_foe(std::array<int, 5> skills) {
    ElfPet p = make_pet(2, "对手", skills, 100, /*atk=*/1, kMaxHp);
    accuracy_100(p, /*must_hit=*/false);
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    // ══ T1700 1932 免疫反弹除{1}外（水晶之匿 [5,排2]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kShuiJing, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // a) 排除码：冻伤(5) → 不免疫、照常落在自身（不反弹）
        (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Frostbite),
                            /*duration=*/3, /*actor=*/1);
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Frostbite)),
              "T1700.a 1932：排除码冻伤照常落地");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Frostbite)),
              "T1700.a 1932：排除码不反弹（对手无冻伤）");
        // b) 掩码内：中毒(1) → 被挡 + 弹回对手
        (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison),
                            /*duration=*/3, /*actor=*/1);
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T1700.b 1932：掩码内中毒被免疫");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Poison)),
              "T1700.b 1932：中毒弹回对手");
        // c) 窗过期（[4] → R1..R4）：R5 起掩码内异常可落地
        for (int r = 0; r < 4; ++r) {
            b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
            b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
            b.drive();
        }
        (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison),
                            /*duration=*/3, /*actor=*/1);
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T1700.c 1932：窗过期后中毒落地");
        std::cout << "  [T1700] 1932 排除码弹控 通过\n";
    }

    // ══ T1701 1687 控制类弹控（灵威神谕 [3]）══
    {
        ScenarioBattle b(
            make_party(mk_self({kLingWei, kPaiDa, kPaiDa, kPaiDa, kPaiDa})),
            make_party(mk_foe({kPaiDa, kPaiDa, kPaiDa, kPaiDa, kPaiDa})));
        b.start();
        BattleContext& ctx = b.ctx();
        b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);   // R1 挂窗
        b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
        b.drive();
        // a) 控制类：麻痹(0) → 被挡 + 弹回对手
        (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Paralysis),
                            /*duration=*/3, /*actor=*/1);
        CHECK(!ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T1701.a 1687：控制类麻痹被免疫");
        CHECK(ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Paralysis)),
              "T1701.a 1687：麻痹弹回对手");
        // b) 非控制：中毒(1) → 不免疫、照常落在自身（只免控制类）
        (void)apply_anomaly(&ctx, 0, static_cast<int>(AbnormalStatusId::Poison),
                            /*duration=*/3, /*actor=*/1);
        CHECK(ctx.has_active_abnormal_status(0, static_cast<int>(AbnormalStatusId::Poison)),
              "T1701.b 1687：非控制中毒照常落地");
        CHECK(!ctx.has_active_abnormal_status(1, static_cast<int>(AbnormalStatusId::Poison)),
              "T1701.b 1687：非控制不反弹（对手无中毒）");
        std::cout << "  [T1701] 1687 控制类弹控 通过\n";
    }

    std::cout << "scenario_1700 PASS (J80 弹控变体族 1932/1687)" << std::endl;
    return 0;
}
