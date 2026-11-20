// 场景 115：药剂体系（battle_items 数据驱动，2026-09-22）。
//   口径验证（seer-robot.cpp 头注）：
//   R1 回血钳上限：hp=max-5 嗑 +20 → 恰为 max（不能超）。
//   R2 封回血不拦嗑药：HEAL_BLOCK 票活跃时嗑药照样回血（嗑药不走恢复原语）。
//   R3 药剂反噬：POTION_BACKLASH 票命中 → "+20" 反转为扣 20（钳 0 可致死；本场景不触死）。
//   R4 单品解异常：状态解除药剂-烧伤(300614) 只清烧伤槽，中毒(直接灌的)保留。
//   R5 组合药剂：修的神奇药水(300749) = +170 体力（钳上限）+ 解全部异常。
//   R6 龙的振奋药水(300750) = +5 PP（钳 maxPP）+ 解能力下降（负等级清零 + 视图同步）。
//   R7 库存/数据门：数量 0 拒绝；battle_items 表没有的 id 拒绝（扣数不发生）。
//   R8 双回药剂：全能恢复药剂(300701) +150 体力 +3 PP 双钳上限。
//   全部直接调 SeerRobot::use_medicine 验结算；FSM index 解析路径由 025/068/074 等既有场景覆盖。
#include <cassert>
#include <iostream>

#include <abnormal-system/abnormal-types.h>
#include <entities/pet_factory.h>
#include <effects/continuousEffect.h>

#include "scenario_harness.h"

namespace {

constexpr int kSkillZhuangJi = 10001;  // 撞击（物理攻击，不参与断言，只占位）
constexpr int kItemHp20 = 300011;      // 初级体力药剂 +20 体力
constexpr int kItemPp5 = 300016;       // 初级活力药剂 +5 PP
constexpr int kItemCureBurn = 300614;  // 状态解除药剂-烧伤（RemoveMonStat=2）
constexpr int kItemAllInOne = 300701;  // 全能恢复药剂 +150 体力 +3 PP
constexpr int kItemXiu = 300749;       // 修的神奇药水 +170 体力 + 解全异常
constexpr int kItemLong = 300750;      // 龙的振奋药水 +5 PP + 解能力下降

}  // namespace

int main() {
    if (!PetFactory::initialize_runtime_data()) {
        std::cerr << "failed to init runtime data" << std::endl;
        return 1;
    }

    auto pet0 = scenario::make_pet(1, "药罐子",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/500);
    auto pet1 = scenario::make_pet(2, "对手",
                                   {kSkillZhuangJi, kSkillZhuangJi, kSkillZhuangJi,
                                    kSkillZhuangJi, kSkillZhuangJi},
                                   /*speed=*/100, /*atk=*/100, /*hp=*/500);
    scenario::ScenarioBattle battle(scenario::make_party(pet0), scenario::make_party(pet1));
    battle.start();
    BattleContext& ctx = battle.ctx();
    auto& robot = ctx.seerRobot[0];
    const int max_hp = ctx.getPet(0).numericalBase[NumericalPropertyIndex::HP];
    CHECK_EQ(max_hp, 500, "make_pet 的 hp=500 应落在 HP 数值槽");

    // ── R1 回血钳上限 ──
    {
        robot.medicines = {{kItemHp20, 1}};
        ctx.getPet(0).hp = max_hp - 5;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemHp20));
        CHECK_EQ(ctx.getPet(0).hp, max_hp, "R1) hp=495 嗑 +20 → 钳到 max=500");
        CHECK_EQ(robot.medicines[kItemHp20], 0, "R1) 库存 1→0");
        std::cout << "  [R1] 回血钳上限通过 (495 -> " << ctx.getPet(0).hp << ")" << std::endl;
    }

    // ── R2 封回血不拦嗑药 ──
    {
        robot.medicines = {{kItemHp20, 1}};
        ctx.grant_immunity(/*owner=*/0, ImmunityType::HEAL_BLOCK, coverage_all(),
                           /*anomaly_mask=*/0, /*duration_rounds=*/0, /*source_id=*/990115);
        ctx.getPet(0).hp = max_hp - 50;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemHp20));
        CHECK_EQ(ctx.getPet(0).hp, max_hp - 30, "R2) 封回血活跃仍 +20（嗑药不走恢复原语）");
        std::cout << "  [R2] 封回血不拦嗑药通过 (" << (max_hp - 50) << " -> "
                  << ctx.getPet(0).hp << ")" << std::endl;
    }

    // ── R3 药剂反噬（预留票：手工授予，无自然注册者）──
    {
        robot.medicines = {{kItemHp20, 2}};
        const int backlash_id = ctx.grant_potion_backlash(
            /*source=*/1, /*source_effect_id=*/990116, /*target=*/0, /*rounds=*/0);
        REQUIRE(ctx.has_potion_backlash(0));
        ctx.getPet(0).hp = 100;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemHp20));
        CHECK_EQ(ctx.getPet(0).hp, 80, "R3) 反噬命中：+20 回复反转为 -20 扣血");
        ctx.getPet(0).hp = 10;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemHp20));
        CHECK(ctx.getPet(0).hp == 0, "R3) 反噬扣血钳 0（可致死，不触发击败时点）");
        ctx.revoke_potion_backlash(backlash_id);   // 撤票，别泄漏进后续用例
        CHECK(!ctx.has_potion_backlash(0), "R3) 撤销后查不到");
        std::cout << "  [R3] 药剂反噬通过 (100->80, 10->" << ctx.getPet(0).hp << ")" << std::endl;
    }

    // ── R4 单品解异常：只清热槽 ──
    {
        robot.medicines = {{kItemCureBurn, 1}};
        ctx.getPet(0).hp = 100;   // R3 反噬致死钳 0 → 复位（已死不能嗑）
        ctx.set_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Burn), 99);
        ctx.set_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Poison), 99);
        REQUIRE(robot.use_medicine(&ctx, 0, kItemCureBurn));
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Burn)),
                 0, "R4) 烧伤槽被直改清零");
        CHECK(ctx.get_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Poison)) > 0,
              "R4) 中毒不在本药剂范围 → 保留");
        std::cout << "  [R4] 单品解异常通过（清热留毒）" << std::endl;
    }

    // ── R5 组合药剂：修的神奇药水（+170 体力 + 解全异常）──
    {
        robot.medicines = {{kItemXiu, 1}};
        ctx.set_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Burn), 99);
        ctx.set_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Fear), 99);
        ctx.getPet(0).hp = 100;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemXiu));
        CHECK_EQ(ctx.getPet(0).hp, 270, "R5) +170 体力直加（无反噬票）");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Burn)),
                 0, "R5) 解全异常：烧伤清");
        CHECK_EQ(ctx.get_abnormal_status_end_round(0, static_cast<int>(AbnormalStatusId::Fear)),
                 0, "R5) 解全异常：害怕清");
        std::cout << "  [R5] 修的神奇药水通过 (100->" << ctx.getPet(0).hp << ", 异常全清)"
                  << std::endl;
    }

    // ── R6 龙的振奋药水（+5 PP + 解能力下降）──
    {
        robot.medicines = {{kItemLong, 1}};
        ctx.ability_levels[0][0] = -2;   // 本体负等级
        ctx.ws.view_levels[0][0] = -2;   // 视图同步前
        ctx.ability_levels[0][1] = 1;    // 正等级不动
        ctx.getPet(0).skills[0].pp = 2;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemLong));
        CHECK_EQ(ctx.ability_levels[0][0], 0, "R6) 负等级清零");
        CHECK_EQ(ctx.ws.view_levels[0][0], 0, "R6) 视图同步清零（公式读它）");
        CHECK_EQ(ctx.ability_levels[0][1], 1, "R6) 正等级不受影响");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 7, "R6) PP +5");
        std::cout << "  [R6] 龙的振奋药水通过 (-2/-2 -> 0/0, pp 2->"
                  << ctx.getPet(0).skills[0].pp << ")" << std::endl;
    }

    // ── R7 库存/数据门 ──
    {
        robot.medicines = {{kItemPp5, 0}, {999999, 1}};
        ctx.getPet(0).skills[0].pp = 1;
        CHECK(!robot.use_medicine(&ctx, 0, kItemPp5), "R7) 数量 0 → 拒绝");
        CHECK(!robot.use_medicine(&ctx, 0, 999999), "R7) battle_items 表没有的 id → 拒绝");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 1, "R7) 拒绝路径不产生任何结算");
        std::cout << "  [R7] 库存/数据门通过" << std::endl;
    }

    // ── R8 双回药剂：全能恢复药剂 ──
    {
        robot.medicines = {{kItemAllInOne, 1}};
        ctx.getPet(0).hp = max_hp - 200;
        ctx.getPet(0).skills[0].pp = 1;
        REQUIRE(robot.use_medicine(&ctx, 0, kItemAllInOne));
        CHECK_EQ(ctx.getPet(0).hp, max_hp - 50, "R8) +150 体力");
        CHECK_EQ(ctx.getPet(0).skills[0].pp, 4, "R8) +3 PP");
        std::cout << "  [R8] 全能恢复药剂通过 (hp ->" << ctx.getPet(0).hp << ", pp ->"
                  << ctx.getPet(0).skills[0].pp << ")" << std::endl;
    }

    std::cout << "scenario_115 PASS (药剂体系: 数据驱动+钳上限+反噬+解异常直改槽位)" << std::endl;
    return 0;
}
