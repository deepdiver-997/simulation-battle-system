// 场景 871 —— R1 切片修复验证：effect 843「下{0}回合令自身所有技能先制+{1}」
//（弦月锋华 25848 [191,816,843] args=[4,1,2,2] → 843 切片 [2,2]）。
//
// 背景（R 组工单 R1）：side_effect 表 816 行 arg_count 曾为 0 → 25848 切片
// 整体左移一格，843 吃到 [1,2]（rounds=1）而非 [2,2]（rounds=2）——"下2回合
// 先制+2"实际只盖 1 回合。修复 816 0→1（tools/db_patches/r1/816_0to1.sql）后
// 843 参数归位（4399 弦月锋华「下2回合自身所有技能先制+2」逐字吻合）。
//
// 断言（p0 速度 100 vs 对手 400——无先制加成时对手恒先手）：
//   T1 R1（施放回合，窗口起点 R+1 不含当回合）→ 对手先手；
//   T2 R2/R3（"下2回合"窗）→ p0 先制+2 压过速度差，恒先手（修复前 R3 落空）；
//   T3 R4 窗过期 → 对手恢复先手。
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXianyue = 25848;   // 弦月锋华：属性技 [191,816,843] args=[4,1,2,2]
constexpr int kZhuangJi = 10001;  // 撞击

ElfPet caster_pet() {
    ElfPet p = make_pet(3507, "弦月锋华", {kXianyue, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/20000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet fast_foe_pet() {
    ElfPet p = make_pet(12, "快速对手", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/400, /*atk=*/1, /*hp=*/20000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

bool p0_first(const BattleContext& ctx) {
    return static_cast<int>(ctx.ws.preemptive_right)
        == static_cast<int>(PreemptiveRight::SEER_ROBOT_1);
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(make_party(caster_pet()), make_party(fast_foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    // R1：施放 25848（843 窗口起点 R+1，当回合不加成）
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    CHECK(!p0_first(ctx),
          "T1 前置：施放回合（R1）窗口未开始，快速对手先手（'下'字语义）");

    // R2/R3：先制+2 窗（rounds=2），压过 300 点速度差
    for (int round = 2; round <= 3; ++round) {
        b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
        b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
        b.drive();
        CHECK(p0_first(ctx), (round == 2
              ? "T2 核心：先制窗 R2 生效，p0 先手（修复前后均过）"
              : "T2 核心：先制窗 R2+R3 两回合（rounds=2；修复前切片 [1,2] 只盖 R2）"));
    }

    // R4：窗过期 → 速度差回归，对手先手
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    CHECK(!p0_first(ctx), "T3 核心：rounds=2 窗过期，快速对手恢复先手");

    std::cout << "  [871] 843 下2回合先制+2 通过（R1 对手先手 → R2/R3 p0 先手 → R4 对手先手）\n";
    return 0;
}
