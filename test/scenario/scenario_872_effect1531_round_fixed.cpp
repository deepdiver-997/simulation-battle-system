// 场景 872 —— R1 切片修复验证：effect 1531「{0}回合内每回合使用技能附加{1}{2}」
//（神之束缚 27020 [1440,936,1531,843] args=[2,100,7,2,3,300,0,2,2] → 1531 切片
// [3,300,0] = 3 回合内每回合附加 300 点固定伤害）。
//
// 背景（R 组工单 R1）：side_effect 表 1531 行 arg_count 曾为 2（effect_info
// args_num=3）→ 切片少 1 参 → 引擎 effect_1531 守卫 int_count<5 恒成立 → 静默
// no-op，且同技 843 参数错位。修复 2→3（tools/db_patches/r1/1531_2to3.sql）后
// 激活（六载体 [3,300,0] 模式跨效果对齐全验证，见 R1 状态列）。
//
// 断言：
//   T1 R1 挂 27020（属性技无伤害阶段）→ R2 出招附加 300 固定伤（掉血≈撞击基线+300）；
//   T2 R4 窗口过期（rounds=3）→ 出招不再附加（掉血回到撞击基线 1~6）。
//   神之束缚 co-effects：1440=[2]、936=[100,7,2]（100%疲惫2回合，无伤害面）、
//   843=[2,2] 先制+2——均不产生固定伤害，带室断言可隔离 1531 附加量。
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kShenzhi = 27020;   // 神之束缚：属性技，1531 窗 [3,300,0]
constexpr int kZhuangJi = 10001;  // 撞击：物攻 35（atk=1 时基线 1~2）

ElfPet caster_pet() {
    ElfPet p = make_pet(2846, "施放方", {kShenzhi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/20000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet sponge_pet() {
    ElfPet p = make_pet(12, "海绵", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/100, /*atk=*/1, /*hp=*/200000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    ScenarioBattle b(make_party(caster_pet()), make_party(sponge_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    // R1：挂 27020（属性技，自身无伤害）
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    const int p1_after_r1 = ctx.getPet(1).hp;

    // R2：窗口内出招 → 附加 300 固定伤
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    const int p1_r2_loss = p1_after_r1 - ctx.getPet(1).hp;
    CHECK(p1_r2_loss >= 295 && p1_r2_loss <= 320,
          "T1 核心：1531 窗口内每回合出招附加 300 固定伤（修复前恒为基线 1~2）");

    // R4：窗口过期（rounds=3）→ 出招回到撞击基线
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);   // R3 过渡
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    const int p1_before_r4 = ctx.getPet(1).hp;
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 1);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    const int p1_r4_loss = p1_before_r4 - ctx.getPet(1).hp;
    CHECK(p1_r4_loss >= 1 && p1_r4_loss <= 8,
          "T2 核心：窗口过期后不再附加（掉血回到撞击基线）");

    std::cout << "  [872] 1531 每回合附加 通过（R2 窗内掉 " << p1_r2_loss
              << "；R4 过期掉 " << p1_r4_loss << "）\n";
    return 0;
}
