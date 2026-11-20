// 场景 878 —— R1 切片修复验证：effect 816「免疫对手下{0}次攻击技能造成的伤害
// 并附加免疫量的真实伤害」（弦月锋华 25848 [191,816,843] args=[4,1,2,2]）。
// （原 870；2026-10-01 X4 让号改 878——与 h10 misc_family 双号，870 归 misc_family。）
//
// 背景（R 组工单 R1）：side_effect 表 816 行 arg_count 曾为 0（与 effect_info
// args_num=1 不一致）→ 切片跳过业务参 → 引擎 effect_816 守卫 int_count<3
// 恒成立 → 816 静默 no-op。修复 0→1（tools/db_patches/r1/816_0to1.sql）后激活。
//
// 断言：
//   T1 R1 挂 25848（属性技无伤害阶段）→ R2 对手攻击被免（掉血=0）且对手
//      吃等量真伤反弹（掉血>0，量级=本次攻击伤害随机样本）；
//   T2 R3 票已消费 → 对手攻击恢复命中（掉血>0），且量级与 R2 反弹量同带
//      （同一技能 85%~100% 浮动，两样本偏差 ≤18%+2）。
#include <iostream>

#include <entities/pet_factory.h>

#include "scenario_harness.h"

using namespace scenario;

namespace {

constexpr int kXianyue = 25848;   // 弦月锋华：属性技 [191,816,843] args=[4,1,2,2]
constexpr int kZhuangJi = 10001;  // 撞击：物攻 35

ElfPet caster_pet() {
    ElfPet p = make_pet(3507, "弦月锋华", {kXianyue, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/300, /*atk=*/1, /*hp=*/20000);
    for (Skills& s : p.skills) {
        s.must_hit = 1;
        s.accuracy = 100;
    }
    return p;
}

ElfPet foe_pet() {
    ElfPet p = make_pet(12, "对手", {kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi, kZhuangJi},
                        /*speed=*/100, /*atk=*/300, /*hp=*/20000);
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

    ScenarioBattle b(make_party(caster_pet()), make_party(foe_pet()));
    b.start();
    BattleContext& ctx = b.ctx();

    // R1：挂 25848（属性技无伤害阶段，不进伤害管线——基线干净）
    b.submit_operation(0, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.submit_operation(1, BattleFsm::ActionType::NONE, 0);
    b.drive();
    const int p0_after_r1 = ctx.getPet(0).hp;
    const int p1_after_r1 = ctx.getPet(1).hp;

    // R2：对手攻击 → 816 免疫（掉血=0）+ 等量真伤反弹（对手掉血>0）
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int p0_r2_loss = p0_after_r1 - ctx.getPet(0).hp;
    const int p1_r2_loss = p1_after_r1 - ctx.getPet(1).hp;
    CHECK_EQ(p0_r2_loss, 0,
             "T1 核心：816 免疫生效——R1 挂票后 R2 挨攻击 0 伤害（修复前恒掉血）");
    CHECK(p1_r2_loss > 0,
          "T1 核心：附加免疫量的真实伤害——攻击方吃反弹（修复前为 0）");

    // R3：票已消费 → 恢复受击；且 R3 实伤与 R2 反弹量同带（同一技能随机样本）
    const int p0_before_r3 = ctx.getPet(0).hp;
    b.submit_operation(0, BattleFsm::ActionType::NONE, 0);
    b.submit_operation(1, BattleFsm::ActionType::SELECT_SKILL, 0);
    b.drive();
    const int p0_r3_loss = p0_before_r3 - ctx.getPet(0).hp;
    CHECK(p0_r3_loss > 0, "T2 核心：1 次免疫已消费，R3 恢复正常受击");
    const int band = p0_r3_loss * 18 / 100 + 2;
    CHECK(p1_r2_loss <= p0_r3_loss + band && p1_r2_loss >= p0_r3_loss - band,
          "T2 量级：反弹量=免疫量（与 R3 同技实伤样本同带）");

    std::cout << "  [878] 816 免疫+真伤反弹 通过（R2 免疫掉 " << p0_r2_loss
              << "、反弹 " << p1_r2_loss << "；R3 实伤 " << p0_r3_loss << "）\n";
    return 0;
}
